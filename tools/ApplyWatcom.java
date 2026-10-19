// Headless Ghidra script: Watcom calling convention + recovered symbols.
//
//   -preScript  ApplyWatcom.java pre [names.tsv]   before auto-analysis:
//        installs the __watcall prototype and applies names/signatures/data labels
//   -postScript ApplyWatcom.java post [names.tsv]   after auto-analysis:
//        re-applies the symbols, then gives every remaining function __watcall (or __stdcall when its RET pops
//        exactly the stack arguments of a callback/API-style function)
//
// names.tsv is written by tools/watcom_names.py or tools/match_functions.py.
//@category A2Racer

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.database.SpecExtension;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.listing.Function.FunctionUpdateType;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;

public class ApplyWatcom extends GhidraScript {

	static final String WATCALL = "__watcall";
	static final String CC_TAG = "CC=";  // tag prefix recording the convention from symbols

	static final String WATCALL_XML =
		"<prototype name=\"__watcall\" extrapop=\"unknown\" stackshift=\"4\">\n" +
		"  <input>\n" +
		"    <pentry minsize=\"1\" maxsize=\"4\"><register name=\"EAX\"/></pentry>\n" +
		"    <pentry minsize=\"1\" maxsize=\"4\"><register name=\"EDX\"/></pentry>\n" +
		"    <pentry minsize=\"1\" maxsize=\"4\"><register name=\"EBX\"/></pentry>\n" +
		"    <pentry minsize=\"1\" maxsize=\"4\"><register name=\"ECX\"/></pentry>\n" +
		"    <pentry minsize=\"1\" maxsize=\"500\" align=\"4\"><addr offset=\"4\" space=\"stack\"/></pentry>\n" +
		"  </input>\n" +
		"  <output>\n" +
		"    <pentry minsize=\"4\" maxsize=\"10\" metatype=\"float\"><register name=\"ST0\"/></pentry>\n" +
		"    <pentry minsize=\"1\" maxsize=\"4\"><register name=\"EAX\"/></pentry>\n" +
		"    <pentry minsize=\"5\" maxsize=\"8\"><addr space=\"join\" piece1=\"EDX\" piece2=\"EAX\"/></pentry>\n" +
		"  </output>\n" +
		"  <unaffected>\n" +
		"    <register name=\"ESP\"/><register name=\"EBP\"/><register name=\"ESI\"/>\n" +
		"    <register name=\"EDI\"/><register name=\"EBX\"/><register name=\"ECX\"/>\n" +
		"    <register name=\"EDX\"/>\n" +
		"  </unaffected>\n" +
		"</prototype>\n";

	private DataTypeManager winArchive;

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		String mode = args.length > 0 ? args[0] : "post";
		if (mode.equals("pre")) {
			new SpecExtension(currentProgram).addReplaceCompilerSpecExtension(WATCALL_XML, monitor);
			if (args.length > 1) {
				applyNames(new File(args[1]));
			}
		}
		else {
			// Analysis can re-create functions and drop signatures applied before it ran,
			// so apply the symbols again before settling conventions.
			if (args.length > 1) {
				applyNames(new File(args[1]));
			}
			assignConventions();
		}
	}

	// ---- pre: names, signatures, data -------------------------------------------------

	private void applyNames(File tsv) throws Exception {
		openWindowsArchive();
		int funcs = 0, typed = 0, labels = 0, data = 0, failed = 0;
		Set<Address> named = new HashSet<>();
		try (BufferedReader r = new BufferedReader(new FileReader(tsv))) {
			String line = r.readLine(); // header
			while ((line = r.readLine()) != null) {
				String[] c = line.split("\t", -1);
				Address a = toAddr(c[0]);
				String kind = c[1], name = c[2], cc = c[3], ret = c[4], params = c[5], dtype = c[6];
				if (kind.equals("local")) {
					continue; // variable renames are applied by ExportDecompiled.java
				}
				try {
					if (kind.equals("function") && !named.contains(a)) {
						Function f = getFunctionAt(a);
						if (f == null && getFunctionContaining(a) != null &&
							getInstructionAt(a) != null) {
							// Analysis merged this function into its predecessor; split it off.
							Function outer = getFunctionContaining(a);
							Address end = outer.getBody().getMaxAddress();
							outer.setBody(new ghidra.program.model.address.AddressSet(
								outer.getEntryPoint(), a.previous()));
						}
						if (f == null) {
							disassemble(a);
							f = createFunction(a, name);
						}
						if (f == null) {
							failed++;
							createLabel(a, name, true, SourceType.IMPORTED);
							continue;
						}
						f.setName(name, SourceType.IMPORTED);
						named.add(a);
						funcs++;
						if (!ret.isEmpty() && !params.equals("?")) {
							setSignature(f, cc, ret, params);
							typed++;
						}
						else if (!cc.isEmpty()) {
							f.setCallingConvention(cc);
						}
						if (!cc.isEmpty() || !ret.isEmpty()) {
							f.addTag(CC_TAG + (cc.isEmpty() ? WATCALL : cc));
						}
					}
					else if (kind.equals("data")) {
						createLabel(a, name, !named.contains(a), SourceType.IMPORTED);
						named.add(a);
						data++;
						if (!dtype.isEmpty()) {
							DataType dt = resolve(dtype);
							if (dt != null && dt.getLength() > 0 && getDataAt(a) == null &&
								getInstructionAt(a) == null) {
								try {
									createData(a, dt);
								}
								catch (Exception e) {
									// conflicting data already defined there; keep the label only
								}
							}
						}
					}
					else {
						createLabel(a, name, !named.contains(a), SourceType.IMPORTED);
						labels++;
					}
				}
				catch (Exception e) {
					failed++;
					printerr("ApplyWatcom: " + c[0] + " " + name + ": " + e.getMessage());
				}
			}
		}
		println("ApplyWatcom names: " + funcs + " functions (" + typed + " typed), " + data + " data, " +
			labels + " labels, " + failed + " failed");
	}

	private void setSignature(Function f, String cc, String ret, String params) throws Exception {
		List<ParameterImpl> ps = new ArrayList<>();
		int i = 1;
		if (!params.isEmpty() && !params.equals("-")) {
			for (String p : params.split("\\|")) {
				// "type:name" names the parameter; a bare type keeps param_N.
				String[] tn = p.split(":", 2);
				String pname = tn.length > 1 ? tn[1] : "param_" + i;
				ps.add(new ParameterImpl(pname, resolve(tn[0]), currentProgram,
					tn.length > 1 ? SourceType.USER_DEFINED : SourceType.DEFAULT));
				i++;
			}
		}
		f.updateFunction(cc.isEmpty() ? WATCALL : cc,
			new ReturnParameterImpl(resolve(ret), currentProgram), ps,
			FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.IMPORTED);
	}

	private void openWindowsArchive() {
		File gdt = new File(System.getenv("GHIDRA_INSTALL_DIR"),
			"Ghidra/Features/Base/data/typeinfo/win32/windows_vs12_32.gdt");
		try {
			winArchive = FileDataTypeManager.openFileArchive(gdt, false);
		}
		catch (Exception e) {
			printerr("ApplyWatcom: cannot open " + gdt + ": " + e.getMessage());
		}
	}

	private DataType resolve(String t) {
		int depth = 0;
		while (t.endsWith("*")) {
			depth++;
			t = t.substring(0, t.length() - 1);
		}
		DataType dt = base(t);
		for (int k = 0; k < depth; k++) {
			dt = new PointerDataType(dt, currentProgram.getDataTypeManager());
		}
		return dt;
	}

	private DataType base(String t) {
		switch (t) {
			case "void": return VoidDataType.dataType;
			case "char": return CharDataType.dataType;
			case "uchar": return UnsignedCharDataType.dataType;
			case "short": return ShortDataType.dataType;
			case "ushort": return UnsignedShortDataType.dataType;
			case "int": return IntegerDataType.dataType;
			case "uint": return UnsignedIntegerDataType.dataType;
			case "long": return LongDataType.dataType;
			case "ulong": return UnsignedLongDataType.dataType;
			case "longlong": return LongLongDataType.dataType;
			case "ulonglong": return UnsignedLongLongDataType.dataType;
			case "float": return FloatDataType.dataType;
			case "double": return DoubleDataType.dataType;
			case "long double": return LongDoubleDataType.dataType;
			case "bool": return BooleanDataType.dataType;
			case "wchar_t": return WideCharDataType.dataType;
		}
		DataTypeManager dtm = currentProgram.getDataTypeManager();
		List<DataType> found = new ArrayList<>();
		dtm.findDataTypes(t, found);
		if (found.isEmpty() && winArchive != null) {
			winArchive.findDataTypes(t, found);
		}
		if (!found.isEmpty()) {
			return dtm.resolve(found.get(0), DataTypeConflictHandler.KEEP_HANDLER);
		}
		// Game-specific struct: placeholder until its layout is recovered.
		return dtm.addDataType(new StructureDataType(new CategoryPath("/A2Racer"), t, 0, dtm),
			DataTypeConflictHandler.KEEP_HANDLER);
	}

	// ---- post: calling conventions for everything else --------------------------------

	private void assignConventions() throws Exception {
		int wat = 0, std = 0, fixed = 0;
		FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
		while (it.hasNext() && !monitor.isCancelled()) {
			Function f = it.next();
			if (f.isExternal() || f.isThunk()) {
				continue;
			}
			int pop = stackPop(f);
			String intended = f.getTags().stream().map(FunctionTag::getName)
				.filter(n -> n.startsWith(CC_TAG)).map(n -> n.substring(CC_TAG.length()))
				.findFirst().orElse(null);
			if (intended != null) {
				// Analysis may have applied a standard-library prototype (fopen, fclose, ...)
				// with that library's convention. Keep its types, restore Watcom's register
				// passing; variadic functions really are passed on the stack. A __watcall
				// function whose RET pops exactly 4 bytes per parameter is really __stdcall
				// (window procedures and other callbacks).
				int n = f.getParameterCount();
				String want = f.hasVarArgs() ? "__cdecl" : intended;
				if (want.equals(WATCALL) && n > 0 && n <= 4 && pop == 4 * n) {
					want = "__stdcall";
				}
				if (!want.equals(f.getCallingConventionName())) {
					restorage(f, want);
					fixed++;
				}
				continue;
			}
			if (pop > 0 && pop >= 16 && f.getName().matches(".*(Proc|Callback|WinMain).*")) {
				f.setCallingConvention("__stdcall");
				std++;
			}
			else {
				f.setCallingConvention(WATCALL);
				wat++;
			}
		}
		println("ApplyWatcom post: " + wat + " __watcall, " + std + " __stdcall, " + fixed +
			" symbol-derived conventions restored or corrected");
	}

	private void restorage(Function f, String cc) throws Exception {
		List<ParameterImpl> ps = new ArrayList<>();
		for (Parameter p : f.getParameters()) {
			ps.add(new ParameterImpl(p.getName(), p.getDataType(), currentProgram));
		}
		f.updateFunction(cc, new ReturnParameterImpl(f.getReturnType(), currentProgram), ps,
			FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, f.getSignatureSource());
	}

	/** Bytes popped by the function's RET imm16 (0 for a plain RET or none found). */
	private int stackPop(Function f) {
		InstructionIterator ii = currentProgram.getListing().getInstructions(f.getBody(), true);
		while (ii.hasNext()) {
			Instruction in = ii.next();
			if (in.getMnemonicString().equalsIgnoreCase("RET") && in.getNumOperands() == 1) {
				Object[] o = in.getOpObjects(0);
				if (o.length == 1 && o[0] instanceof ghidra.program.model.scalar.Scalar) {
					return (int) ((ghidra.program.model.scalar.Scalar) o[0]).getValue();
				}
			}
		}
		return 0;
	}
}
