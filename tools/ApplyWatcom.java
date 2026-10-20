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
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
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
	static final String CC_TAG = "CC=";
	// Marks a signature taken from symbols; InferSignatures.java leaves those alone.
	static final String SIG_TAG = "SIGNATURE";
	static final boolean DEBUG = Boolean.getBoolean("a2.debug");  // tag prefix recording the convention from symbols

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

	/**
	 * __watcallN: Watcom's register convention for a function with N register parameters.
	 * A Watcom function preserves every register except EAX (the result) and the argument
	 * registers it takes, so the argument registers past N are unaffected by a call.
	 * InferSignatures.java picks N per function.
	 */
	static String watcallXml(int n) {
		String[] regs = { "EAX", "EDX", "EBX", "ECX" };
		StringBuilder unaffected = new StringBuilder(
			"    <register name=\"ESP\"/><register name=\"EBP\"/><register name=\"ESI\"/>" +
				"<register name=\"EDI\"/>");
		for (int i = Math.max(n, 1); i < 4; i++) {
			unaffected.append("<register name=\"").append(regs[i]).append("\"/>");
		}
		return WATCALL_XML.replace("name=\"__watcall\"", "name=\"__watcall" + n + "\"")
			.replaceAll("(?s)<unaffected>.*</unaffected>",
				"<unaffected>\n" + unaffected + "\n  </unaffected>");
	}

	/**
	 * Watcom variadic functions take every argument on the stack (the caller pops them)
	 * but, unlike Microsoft's __cdecl, preserve every register except EAX.
	 */
	static final String VARARGS = "__watcall_varargs";
	static final String VARARGS_XML =
		"<prototype name=\"" + VARARGS + "\" extrapop=\"4\" stackshift=\"4\">\n" +
		"  <input>\n" +
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
			SpecExtension spec = new SpecExtension(currentProgram);
			spec.addReplaceCompilerSpecExtension(WATCALL_XML, monitor);
			for (int n = 0; n <= 4; n++) {
				spec.addReplaceCompilerSpecExtension(watcallXml(n), monitor);
			}
			spec.addReplaceCompilerSpecExtension(VARARGS_XML, monitor);
			// This heuristic marks a function non-returning when calls to it sit right before
			// another function's start. With every symbol created up front that misfires (it
			// marked debugmsg non-returning and cut code after 164 call sites). Library exits
			// are still covered by the "Non-Returning Functions - Known" analyzer.
			setAnalysisOption(currentProgram, "Non-Returning Functions - Discovered", "false");
			if (args.length > 3) {
				parseHeader(new File(args[3]));
			}
			if (args.length > 2) {
				defineStructs(new File(args[2]));
			}
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
			nameSharedTails(args.length > 2 && args[2].equals("complete"));
			defineShortStrings();
			defineFloatConstants();
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
							f.addTag(SIG_TAG);
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

	/** Builds the packed structures listed in structs.tsv (struct, offset, type, name, comment). */
	/** Parse a plain C header (symbols/directx.h) into the program's data types. */
	private void parseHeader(File h) throws Exception {
		if (winArchive == null) {
			openWindowsArchive();
		}
		ghidra.app.util.cparser.C.CParser parser = new ghidra.app.util.cparser.C.CParser(
			currentProgram.getDataTypeManager(), true,
			winArchive == null ? null : new DataTypeManager[] { winArchive });
		try (java.io.InputStream in = new java.io.FileInputStream(h)) {
			parser.parse(in);
		}
		// The parser drops "__stdcall" from function-pointer members; COM methods are all
		// __stdcall (the callee pops its arguments), and the stack depth after every COM
		// call depends on it. Name the anonymous signatures after their slot as well.
		DataTypeManager dtm = currentProgram.getDataTypeManager();
		for (DataType comp : parser.getComposites().values()) {
			DataType mine = dtm.getDataType(comp.getDataTypePath());
			if (!(mine instanceof Structure) || !mine.getName().endsWith("Vtbl")) {
				continue;
			}
			for (DataTypeComponent c : ((Structure) mine).getDefinedComponents()) {
				if (c.getDataType() instanceof Pointer &&
					((Pointer) c.getDataType()).getDataType() instanceof FunctionDefinition) {
					FunctionDefinition fd =
						(FunctionDefinition) ((Pointer) c.getDataType()).getDataType();
					fd.setCallingConvention("__stdcall");
					try {
						fd.setName(mine.getName().replace("Vtbl", "") + "_" + c.getFieldName());
					}
					catch (Exception e) {
						// keep the generated name
					}
				}
			}
		}
		println("ApplyWatcom: " + parser.getComposites().size() + " structures and " +
			parser.getTypes().size() + " types parsed from " + h.getName());
	}

	private void defineStructs(File tsv) throws Exception {
		if (!tsv.exists()) {
			return;
		}
		if (winArchive == null) {
			openWindowsArchive();
		}
		Map<String, List<String[]>> fields = new java.util.LinkedHashMap<>();
		try (BufferedReader r = new BufferedReader(new FileReader(tsv))) {
			String line;
			while ((line = r.readLine()) != null) {
				if (line.startsWith("#") || line.startsWith("struct\t") || line.isBlank()) {
					continue;
				}
				String[] c = line.split("\t", -1);
				fields.computeIfAbsent(c[0], k -> new ArrayList<>()).add(c);
			}
		}
		DataTypeManager dtm = currentProgram.getDataTypeManager();
		for (Map.Entry<String, List<String[]>> e : fields.entrySet()) {
			int size = 0;
			List<Object[]> parsed = new ArrayList<>();
			for (String[] c : e.getValue()) {
				int off = Integer.parseInt(c[1], 16);
				DataType dt = resolveArray(c[2]);
				parsed.add(new Object[] { off, dt, c[3], c.length > 4 ? c[4] : null });
				size = Math.max(size, off + dt.getLength());
			}
			StructureDataType st = new StructureDataType(new CategoryPath("/A2Racer"), e.getKey(), size, dtm);
			for (Object[] f : parsed) {
				DataType dt = (DataType) f[1];
				st.replaceAtOffset((Integer) f[0], dt, dt.getLength(), (String) f[2], (String) f[3]);
			}
			dtm.addDataType(st, DataTypeConflictHandler.REPLACE_HANDLER);
			println("ApplyWatcom: struct " + e.getKey() + " (" + size + " bytes, " + parsed.size() + " fields)");
		}
	}

	private DataType resolveArray(String t) {
		java.util.regex.Matcher m = java.util.regex.Pattern.compile("(.*)\\[(\\d+)\\]$").matcher(t);
		if (m.matches()) {
			DataType el = resolve(m.group(1));
			return new ArrayDataType(el, Integer.parseInt(m.group(2)), el.getLength());
		}
		return resolve(t);
	}

	private void setSignature(Function f, String cc, String ret, String params) throws Exception {
		List<ParameterImpl> ps = new ArrayList<>();
		int i = 1;
		boolean varargs = false;
		if (!params.isEmpty() && !params.equals("-")) {
			for (String p : params.split("\\|")) {
				if (p.equals("...")) {
					varargs = true;
					continue;
				}
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
		f.setVarArgs(varargs);
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

	// ---- post: shared tails ----------------------------------------------------------------

	/**
	 * Watcom merges identical function tails: one function jumps into another's epilogue or
	 * error exit. Ghidra splits such a block off as its own FUN_. A default-named function that
	 * is only ever jumped to (never called) is named after the function it physically sits in.
	 */
	private void nameSharedTails(boolean symbolsComplete) throws Exception {
		int n = 0;
		FunctionManager fm = currentProgram.getFunctionManager();
		for (Function f : fm.getFunctions(true)) {
			if (f.getSymbol().getSource() != SourceType.DEFAULT || f.isThunk()) {
				continue;
			}
			boolean jumped = false, called = false;
			for (ghidra.program.model.symbol.Reference r : getReferencesTo(f.getEntryPoint())) {
				// Ghidra re-types a jump to a function entry as a tail call, so look at the
				// instruction itself.
				Instruction in = getInstructionAt(r.getFromAddress());
				String m = in == null ? "" : in.getMnemonicString().toUpperCase();
				if (m.startsWith("J")) {
					jumped = true;
				}
				else {
					called = true; // CALL, or a data/pointer reference: a real entry point
				}
			}
			if (!jumped || called) {
				if (DEBUG) {
					println("tail? " + f.getName() + " jumped=" + jumped + " called=" + called);
				}
				continue;
			}
			if (!symbolsComplete && !looksLikeTail(f)) {
				continue; // probably a real function that is only ever tail-called
			}
			// Owner: the nearest preceding function with a real name.
			Function owner = null;
			FunctionIterator back = fm.getFunctions(f.getEntryPoint(), false);
			while (back.hasNext()) {
				Function g = back.next();
				if (g != f && g.getSymbol().getSource() != SourceType.DEFAULT) {
					owner = g;
					break;
				}
			}
			if (owner == null) {
				continue;
			}
			String base = owner.getName().replaceAll("(_tail(_\\d+)?)+$", "") + "_tail";
			String name = base;
			for (int i = 2; !getGlobalFunctions(name).isEmpty(); i++) {
				name = base + "_" + i;
			}
			f.setName(name, SourceType.ANALYSIS);
			n++;
		}
		println("ApplyWatcom post: " + n + " shared tails named after their owner");
	}

	/**
	 * Without complete symbols a jump-only block may be a real function reached by tail calls.
	 * A shared tail is entered by falling through from its owner, or starts with epilogue code.
	 */
	private boolean looksLikeTail(Function f) {
		Instruction prev = currentProgram.getListing().getInstructionBefore(f.getEntryPoint());
		boolean fallsThrough = prev != null && prev.getMaxAddress().next().equals(f.getEntryPoint()) &&
			!prev.getFlowType().isTerminal() &&
			!(prev.getFlowType().isJump() && prev.getFlowType().isUnConditional());
		Instruction first = getInstructionAt(f.getEntryPoint());
		String m = first == null ? "" : first.getMnemonicString().toUpperCase();
		boolean epilogue = m.equals("POP") || m.equals("LEAVE") ||
			(m.equals("ADD") && first.getDefaultOperandRepresentation(0).equalsIgnoreCase("ESP"));
		return fallsThrough || epilogue;
	}

	// ---- post: short strings -----------------------------------------------------------

	/**
	 * Ghidra's string analyzer skips strings shorter than five characters, so literals such as
	 * "\\", "rb" or "GAME" show up as DAT_ globals. Any undefined location that code refers to
	 * and that holds a short NUL-terminated printable string becomes a string.
	 */
	private void defineShortStrings() throws Exception {
		int n = 0;
		ghidra.program.model.mem.Memory mem = currentProgram.getMemory();
		// Candidate addresses: taken as a value (DATA, or PARAM when passed to a call) and never
		// written (a written location is a buffer, not a constant).
		Set<Address> taken = new HashSet<>(), written = new HashSet<>();
		InstructionIterator ii = currentProgram.getListing().getInstructions(true);
		while (ii.hasNext() && !monitor.isCancelled()) {
			for (ghidra.program.model.symbol.Reference r : ii.next().getReferencesFrom()) {
				ghidra.program.model.symbol.RefType t = r.getReferenceType();
				if (t == ghidra.program.model.symbol.RefType.DATA ||
					t == ghidra.program.model.symbol.RefType.PARAM) {
					taken.add(r.getToAddress());
				}
				else if (t.isWrite()) {
					written.add(r.getToAddress());
				}
			}
		}
		for (Address a : taken) {
			if (written.contains(a) || !a.isMemoryAddress() || mem.getBlock(a) == null ||
				mem.getBlock(a).isExecute() || getInstructionAt(a) != null) {
				continue;
			}
			Data d = getDataAt(a);
			boolean placeholder = d == null || !d.isDefined() ||
				d.getDataType() instanceof Undefined;
			if (!placeholder) {
				continue;
			}
			int len = 0;
			boolean ok = false;
			for (; len < 64; len++) {
				byte b;
				try {
					b = mem.getByte(a.add(len));
				}
				catch (Exception e) {
					break;
				}
				if (b == 0) {
					ok = len > 0;
					break;
				}
				if ((b < 0x20 || b > 0x7e) && b != '\t' && b != '\n' && b != '\r') {
					break;
				}
			}
			if (!ok) {
				continue;
			}
			try {
				// Only placeholder data may be replaced; real typed data stops the string.
				for (int k = 0; k <= len; k++) {
					Data x = getDataAt(a.add(k));
					if (x != null && x.isDefined() && !(x.getDataType() instanceof Undefined)) {
						throw new IllegalStateException("typed data");
					}
				}
				clearListing(a, a.add(len));
				createAsciiString(a, len + 1);
				n++;
			}
			catch (Exception e) {
				// overlaps typed data; leave it
			}
		}
		println("ApplyWatcom post: " + n + " short strings defined");
	}

	// ---- post: floating-point constants -------------------------------------------------

	/**
	 * Watcom keeps float literals in the writable data segment, so the decompiler prints them as
	 * (float)_DAT_xxx instead of their value. A location that FPU instructions only read (never
	 * written anywhere) is typed float/double by operand size and marked constant.
	 */
	private void defineFloatConstants() throws Exception {
		Map<Address, Integer> size = new HashMap<>();
		Set<Address> written = new HashSet<>(), otherUse = new HashSet<>();
		InstructionIterator ii = currentProgram.getListing().getInstructions(true);
		while (ii.hasNext() && !monitor.isCancelled()) {
			Instruction in = ii.next();
			String m = in.getMnemonicString().toUpperCase();
			boolean fpuRead = m.startsWith("F") && !m.startsWith("FST") && !m.startsWith("FI") &&
				!m.startsWith("FN") && !m.startsWith("FLDCW") && !m.startsWith("FLDENV");
			for (ghidra.program.model.symbol.Reference r : in.getReferencesFrom()) {
				Address a = r.getToAddress();
				if (r.getReferenceType().isWrite()) {
					written.add(a);
				}
				else if (fpuRead && r.getReferenceType().isRead()) {
					String op = in.getDefaultOperandRepresentation(r.getOperandIndex()).toLowerCase();
					int sz = op.startsWith("float") || op.startsWith("dword") ? 4
							: op.startsWith("double") || op.startsWith("qword") ? 8 : 0;
					if (sz == 0 || (size.containsKey(a) && size.get(a) != sz)) {
						otherUse.add(a);
					}
					else {
						size.put(a, sz);
					}
				}
				else {
					otherUse.add(a);
				}
			}
		}
		int n = 0;
		ghidra.program.model.mem.Memory mem = currentProgram.getMemory();
		for (Map.Entry<Address, Integer> e : size.entrySet()) {
			Address a = e.getKey();
			if (written.contains(a) || otherUse.contains(a) || mem.getBlock(a) == null ||
				mem.getBlock(a).isExecute()) {
				continue;
			}
			Data d = getDataAt(a);
			if (d != null && d.isDefined() && !(d.getDataType() instanceof Undefined)) {
				continue;
			}
			try {
				clearListing(a, a.add(e.getValue() - 1));
				Data c = createData(a, e.getValue() == 4 ? FloatDataType.dataType : DoubleDataType.dataType);
				ghidra.program.model.data.MutabilitySettingsDefinition.DEF.setChoice(c,
					ghidra.program.model.data.MutabilitySettingsDefinition.CONSTANT);
				n++;
			}
			catch (Exception ex) {
				// overlaps other data; leave it
			}
		}
		println("ApplyWatcom post: " + n + " floating-point constants defined");
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
				String want = f.hasVarArgs() ? VARARGS : intended;
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
