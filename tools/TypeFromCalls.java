// Headless Ghidra script: type variables from the calls they are passed to.
//
//   -postScript TypeFromCalls.java
//
// When the address of a stack local or a global (&local_64, &DAT_00476508) is passed to a
// parameter of type S* where S is a structure, union or array, the variable gets type S;
// a global assigned the result of a call returning S* gets type S*. This picks up the
// DirectX descriptor structures (DDSURFACEDESC, D3DMATERIAL, ...) from the COM method
// prototypes in symbols/directx.h, and game structures from recovered signatures.
// Only variables whose current type is undefined (or a lone scalar) are changed.
//@category A2Racer

import java.util.*;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.*;
import ghidra.program.model.symbol.SourceType;

public class TypeFromCalls extends GhidraScript {

	private DecompInterface decomp;
	private int locals, globals;

	@Override
	protected void run() throws Exception {
		decomp = new DecompInterface();
		decomp.setOptions(new DecompileOptions());
		decomp.toggleSyntaxTree(true);
		decomp.setSimplificationStyle("decompile");
		decomp.openProgram(currentProgram);
		// Indirect calls through typed function pointers (COM methods): the decompiler only
		// honours the callee's stack cleanup (__stdcall) through a call-site override, and
		// without it loses track of the stack after the call.
		int purges = 0;
		for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
			if (!f.isThunk() && !f.isExternal() && !monitor.isCancelled()) {
				purges += setIndirectPurges(f);
			}
		}
		println("TypeFromCalls: signature override at " + purges + " indirect calls");
		// A second round sees calls whose arguments were typed by the first.
		for (int round = 0; round < 2; round++) {
			for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
				if (monitor.isCancelled()) {
					break;
				}
				if (!f.isThunk() && !f.isExternal()) {
					typeFunction(f);
				}
			}
		}
		decomp.dispose();
		println("TypeFromCalls: " + locals + " locals and " + globals + " globals typed");
	}

	private void typeFunction(Function f) {
		DecompileResults r = decomp.decompileFunction(f, 60, monitor);
		if (r == null || r.getHighFunction() == null) {
			return;
		}
		HighFunction hf = r.getHighFunction();
		Map<HighSymbol, DataType> plan = new LinkedHashMap<>();
		Map<Address, DataType> globalPlan = new LinkedHashMap<>();
		Iterator<PcodeOpAST> ops = hf.getPcodeOps();
		while (ops.hasNext()) {
			PcodeOpAST op = ops.next();
			if (op.getOpcode() == PcodeOp.CALL || op.getOpcode() == PcodeOp.CALLIND) {
				List<DataType> params = paramTypes(op);
				for (int i = 1; i < op.getNumInputs() && i - 1 < params.size(); i++) {
					DataType target = pointee(params.get(i - 1));
					if (target != null) {
						consider(hf, op.getInput(i), target, plan, globalPlan);
					}
				}
			}
		}
		for (Map.Entry<HighSymbol, DataType> e : plan.entrySet()) {
			try {
				HighFunctionDBUtil.updateDBVariable(e.getKey(), null, e.getValue(),
					SourceType.ANALYSIS);
				locals++;
			}
			catch (Exception ex) {
				// Overlaps another variable: leave it.
			}
		}
		for (Map.Entry<Address, DataType> e : globalPlan.entrySet()) {
			Address a = e.getKey();
			DataType dt = e.getValue();
			try {
				if (fitsGlobal(a, dt)) {
					clearListing(a, a.add(dt.getLength() - 1));
					createData(a, dt);
					globals++;
				}
			}
			catch (Exception ex) {
				// Overlaps instructions or other data: leave it.
			}
		}
	}

	private int setIndirectPurges(Function f) {
		DecompileResults r = decomp.decompileFunction(f, 60, monitor);
		if (r == null || r.getHighFunction() == null) {
			return 0;
		}
		int n = 0;
		Iterator<PcodeOpAST> ops = r.getHighFunction().getPcodeOps();
		while (ops.hasNext()) {
			PcodeOpAST op = ops.next();
			if (op.getOpcode() != PcodeOp.CALLIND) {
				continue;
			}
			HighVariable hv = op.getInput(0).getHigh();
			DataType dt = hv == null ? null : hv.getDataType();
			while (dt instanceof TypeDef) {
				dt = ((TypeDef) dt).getBaseDataType();
			}
			if (!(dt instanceof Pointer) ||
				!(((Pointer) dt).getDataType() instanceof FunctionDefinition)) {
				continue;
			}
			FunctionDefinition fd = (FunctionDefinition) ((Pointer) dt).getDataType();
			if (!"__stdcall".equals(fd.getCallingConventionName()) || fd.hasVarArgs()) {
				continue;
			}
			Address at = op.getSeqnum().getTarget();
			try {
				HighFunctionDBUtil.writeOverride(f, at, fd);
				n++;
			}
			catch (Exception e) {
				// leave the call as analyzed
			}
		}
		return n;
	}

	/** Parameter types of the called function or function pointer, in argument order. */
	private List<DataType> paramTypes(PcodeOp op) {
		List<DataType> out = new ArrayList<>();
		if (op.getOpcode() == PcodeOp.CALL) {
			Function callee = getFunctionAt(op.getInput(0).getAddress());
			if (callee == null) {
				return out;
			}
			for (Parameter p : callee.getParameters()) {
				out.add(p.getDataType());
			}
			return out;
		}
		HighVariable hv = op.getInput(0).getHigh();
		DataType dt = hv == null ? null : hv.getDataType();
		if (dt instanceof TypeDef) {
			dt = ((TypeDef) dt).getBaseDataType();
		}
		if (dt instanceof Pointer && ((Pointer) dt).getDataType() instanceof FunctionDefinition) {
			for (ParameterDefinition p : ((FunctionDefinition) ((Pointer) dt).getDataType())
					.getArguments()) {
				out.add(p.getDataType());
			}
		}
		return out;
	}

	/** S for a parameter of type S* where S is a sized structure, union or array. */
	private static DataType pointee(DataType dt) {
		while (dt instanceof TypeDef) {
			dt = ((TypeDef) dt).getBaseDataType();
		}
		if (!(dt instanceof Pointer)) {
			return null;
		}
		DataType s = ((Pointer) dt).getDataType();
		DataType base = s;
		while (base instanceof TypeDef) {
			base = ((TypeDef) base).getBaseDataType();
		}
		if ((base instanceof Composite || base instanceof Array) && !base.isZeroLength() &&
			base.getLength() > 4) {
			return s;
		}
		return null;
	}

	/** Record the variable whose address the argument is, if its type is still generic. */
	private void consider(HighFunction hf, Varnode arg, DataType target,
			Map<HighSymbol, DataType> plan, Map<Address, DataType> globalPlan) {
		PcodeOp def = arg.getDef();
		while (def != null && (def.getOpcode() == PcodeOp.COPY || def.getOpcode() == PcodeOp.CAST)) {
			arg = def.getInput(0);
			def = arg.getDef();
		}
		if (def != null && def.getOpcode() == PcodeOp.PTRSUB) {
			Varnode base = def.getInput(0), off = def.getInput(1);
			if (base.isRegister() && off.isConstant()) {
				// &local: stack pointer plus the local's offset.
				Address sa = currentProgram.getAddressFactory().getStackSpace()
					.getAddress((int) off.getOffset()); // 32-bit offset, sign-extended
				HighSymbol sym = hf.getLocalSymbolMap().findLocal(sa, null);
				if (sym == null || plan.containsKey(sym) || sym.getStorage().getMinAddress() == null ||
					!sym.getStorage().getMinAddress().equals(sa)) {
					return; // unknown, or the address points inside the local
				}
				DataType cur = sym.getDataType();
				if (isGeneric(cur) && cur.getLength() <= target.getLength()) {
					plan.put(sym, target);
				}
			}
			else if (base.isConstant() && base.getOffset() == 0 && off.isConstant()) {
				addGlobal(toAddr(off.getOffset()), target, globalPlan);
			}
		}
		else if (arg.isConstant() && arg.getSize() == 4) {
			addGlobal(toAddr(arg.getOffset()), target, globalPlan);
		}
	}

	private void addGlobal(Address a, DataType target, Map<Address, DataType> globalPlan) {
		if (!currentProgram.getMemory().contains(a) || globalPlan.containsKey(a)) {
			return;
		}
		Data d = getDataAt(a);
		if (getInstructionContaining(a) == null && (d == null || isGeneric(d.getDataType()))) {
			globalPlan.put(a, target);
		}
	}

	private static boolean isGeneric(DataType dt) {
		if (dt == null || dt instanceof Undefined || dt instanceof DefaultDataType) {
			return true;
		}
		if (dt instanceof Array) {
			return isGeneric(((Array) dt).getDataType());
		}
		return dt instanceof AbstractIntegerDataType;
	}

	/** True when the global's range holds only undefined bytes or one generic item. */
	private boolean fitsGlobal(Address a, DataType dt) {
		Listing listing = currentProgram.getListing();
		Address end = a.add(dt.getLength() - 1);
		if (!currentProgram.getMemory().contains(end)) {
			return false;
		}
		DataIterator it = listing.getDefinedData(new ghidra.program.model.address.AddressSet(a, end), true);
		while (it.hasNext()) {
			Data d = it.next();
			if (!isGeneric(d.getDataType()) || (!d.getAddress().equals(a) &&
				currentProgram.getSymbolTable().getPrimarySymbol(d.getAddress()) != null &&
				currentProgram.getSymbolTable().getPrimarySymbol(d.getAddress()).getSource() !=
					SourceType.DEFAULT)) {
				return false;
			}
		}
		return true;
	}
}
