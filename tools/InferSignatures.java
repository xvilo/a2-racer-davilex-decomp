// Headless Ghidra script: settle the signatures of functions no symbol describes.
//
//   -postScript InferSignatures.java
//
// Functions tagged SIGNATURE by ApplyWatcom.java keep their recovered or hand-written
// prototype. For the rest:
//   1. parameters  callees first, the decompiler's parameter guess is committed, so a
//                  caller that only passes registers through to an already-settled callee
//                  no longer counts them as its own parameters;
//   2. return      a 64-bit EDX:EAX guess becomes a 32-bit value (nothing in the game
//                  returns 64-bit values outside the tagged runtime), and a function
//                  whose result no call site reads returns void.
// Functions with a locked prototype that still read argument registers (in_EAX, ...) are
// listed, since that usually means a hand-written signature misses parameters.
//@category A2Racer

import java.util.*;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionTag;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.ParameterImpl;
import ghidra.program.model.listing.ReturnParameterImpl;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.pcode.*;
import ghidra.program.model.symbol.SourceType;

public class InferSignatures extends GhidraScript {

	static final String SIG_TAG = "SIGNATURE";
	static final String WATCALL = "__watcall";
	static final List<String> ARG_REGS = Arrays.asList("EAX", "EDX", "EBX", "ECX");

	private DecompInterface decomp;

	@Override
	protected void run() throws Exception {
		decomp = new DecompInterface();
		decomp.setOptions(new DecompileOptions());
		decomp.toggleSyntaxTree(true);
		decomp.setSimplificationStyle("decompile");
		decomp.openProgram(currentProgram);

		List<Function> order = bottomUp();
		Set<Function> targets = new HashSet<>();
		for (Function f : order) {
			if (!f.isThunk() && !f.isExternal() && !hasTag(f, SIG_TAG)) {
				targets.add(f);
			}
		}

		int overrides = overrideVariadicCalls();

		// 1. Register parameter counts from the machine code, then a locked prototype with
		//    that many parameters (types left undefined so the decompiler still infers them).
		Map<Function, Integer> regs = registerParamCounts(order, targets);
		int conventions = 0;
		for (Function f : order) {
			if (f.isThunk() || f.isExternal() || !f.getCallingConventionName().equals(WATCALL)) {
				continue;
			}
			if (targets.contains(f)) {
				int n = regs.get(f), stack = stackPop(f) / 4;
				List<ghidra.program.model.listing.Variable> ps = new ArrayList<>();
				for (int i = 0; i < n + stack; i++) {
					ps.add(new ParameterImpl("param_" + (i + 1), Undefined4DataType.dataType,
						currentProgram, SourceType.DEFAULT));
				}
				f.updateFunction(convention(n, stack),
					new ReturnParameterImpl(Undefined4DataType.dataType, currentProgram), ps,
					Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true,
					SourceType.ANALYSIS);
			}
			else {
				DataType ret = f.getReturnType();
				if (ret.getLength() > 4 && !(ret instanceof AbstractFloatDataType)) {
					continue; // EDX:EAX result: EDX is never preserved
				}
				List<ghidra.program.model.listing.Variable> ps = new ArrayList<>();
				for (ghidra.program.model.listing.Parameter p : f.getParameters()) {
					ps.add(new ParameterImpl(p.getName(), p.getDataType(), currentProgram,
						p.getSource()));
				}
				f.updateFunction(convention(regs.get(f), stackPop(f) / 4),
					new ReturnParameterImpl(ret, currentProgram),
					ps, Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true,
					f.getSignatureSource());
			}
			conventions++;
		}

		// 2. Parameter types and a first return guess, callees first.
		int committed = 0, narrowed = 0;
		for (Function f : order) {
			if (monitor.isCancelled() || !targets.contains(f)) {
				continue;
			}
			HighFunction hf = decompile(f);
			if (hf == null) {
				continue;
			}
			try {
				HighFunctionDBUtil.commitParamsToDatabase(hf, true,
					HighFunctionDBUtil.ReturnCommitOption.COMMIT, SourceType.ANALYSIS);
				committed++;
			}
			catch (Exception e) {
				println("InferSignatures: " + f.getName() + ": " + e.getMessage());
				continue;
			}
			DataType ret = f.getReturnType();
			if (ret.getLength() > 4 && !(ret instanceof AbstractFloatDataType)) {
				f.setReturnType(Undefined4DataType.dataType, SourceType.ANALYSIS);
				narrowed++;
			}
		}

		// 3. Results nobody reads: scan every call site once.
		Map<Function, Boolean> used = new HashMap<>();
		List<String> suspicious = new ArrayList<>();
		for (Function caller : order) {
			if (monitor.isCancelled() || caller.isThunk() || caller.isExternal()) {
				continue;
			}
			HighFunction hf = decompile(caller);
			if (hf == null) {
				continue;
			}
			if (hasTag(caller, SIG_TAG) && readsArgRegisters(hf)) {
				suspicious.add(caller.getName());
			}
			Iterator<PcodeOpAST> ops = hf.getPcodeOps();
			while (ops.hasNext()) {
				PcodeOpAST op = ops.next();
				if (op.getOpcode() != PcodeOp.CALL) {
					continue;
				}
				Function callee = getFunctionAt(op.getInput(0).getAddress());
				if (callee != null && callee.isThunk()) {
					callee = callee.getThunkedFunction(true);
				}
				if (callee == null || !targets.contains(callee)) {
					continue;
				}
				Varnode out = op.getOutput();
				boolean reads = out != null && out.getDescendants().hasNext();
				used.merge(callee, reads, Boolean::logicalOr);
			}
		}
		int voided = 0;
		for (Map.Entry<Function, Boolean> e : used.entrySet()) {
			Function f = e.getKey();
			if (!e.getValue() && !(f.getReturnType() instanceof VoidDataType) &&
				!(f.getReturnType() instanceof AbstractFloatDataType)) {
				f.setReturnType(VoidDataType.dataType, SourceType.ANALYSIS);
				voided++;
			}
		}
		decomp.dispose();
		println("InferSignatures: " + conventions + " functions given __watcallN by register parameters");
		println("InferSignatures: " + overrides + " variadic call sites given their argument count");
		println("InferSignatures: " + committed + " signatures inferred, " + narrowed +
			" 64-bit results narrowed, " + voided + " unread results made void");
		Collections.sort(suspicious);
		if (!suspicious.isEmpty()) {
			println("InferSignatures: symbol signatures that still read argument registers: " +
				String.join(" ", suspicious));
		}
	}

	/**
	 * Give each call to a variadic function its real argument count, read from the
	 * "ADD ESP,n" that pops the arguments. Without it the decompiler reads stack slots
	 * past the arguments - registers the caller saved in its prologue - and takes those
	 * registers for parameters of the caller.
	 */
	private int overrideVariadicCalls() throws Exception {
		int n = 0;
		ghidra.program.model.listing.Listing listing = currentProgram.getListing();
		for (Function callee : currentProgram.getFunctionManager().getFunctions(true)) {
			if (!callee.hasVarArgs()) {
				continue;
			}
			int fixedBytes = 0;
			for (ghidra.program.model.listing.Parameter p : callee.getParameters()) {
				fixedBytes += Math.max(4, p.getLength());
			}
			for (ghidra.program.model.symbol.Reference ref :
					getReferencesTo(callee.getEntryPoint())) {
				if (!ref.getReferenceType().isCall()) {
					continue;
				}
				Address at = ref.getFromAddress();
				Function caller = getFunctionContaining(at);
				ghidra.program.model.listing.Instruction next = listing.getInstructionAfter(at);
				if (caller == null || next == null) {
					continue;
				}
				int popped = poppedBytes(next);
				if (popped < fixedBytes || popped % 4 != 0) {
					continue;
				}
				FunctionDefinitionDataType sig =
					new FunctionDefinitionDataType(callee.getSignature(), currentProgram.getDataTypeManager());
				List<ParameterDefinition> ps = new ArrayList<>(Arrays.asList(sig.getArguments()));
				for (int b = fixedBytes; b < popped; b += 4) {
					ps.add(new ParameterDefinitionImpl("arg" + (ps.size() + 1),
						Undefined4DataType.dataType, null));
				}
				sig.setArguments(ps.toArray(new ParameterDefinition[0]));
				sig.setVarArgs(false);
				sig.setCallingConvention(callee.getCallingConventionName());
				HighFunctionDBUtil.writeOverride(caller, at, sig);
				n++;
			}
		}
		return n;
	}

	/** Bytes released by "ADD ESP,n" (or "LEA ESP,[ESP+n]") right after a call; -1 if none. */
	private static int poppedBytes(ghidra.program.model.listing.Instruction in) {
		String m = in.getMnemonicString().toUpperCase();
		if (!(m.equals("ADD") || m.equals("LEA")) || in.getNumOperands() != 2) {
			return -1;
		}
		String dst = in.getDefaultOperandRepresentation(0);
		if (!dst.equalsIgnoreCase("ESP")) {
			return -1;
		}
		Object[] o = in.getOpObjects(1);
		for (Object x : o) {
			if (x instanceof ghidra.program.model.scalar.Scalar) {
				return (int) ((ghidra.program.model.scalar.Scalar) x).getValue();
			}
		}
		return -1;
	}

	/**
	 * Register parameters per __watcall function: the highest of EAX, EDX, EBX, ECX that is
	 * read before being written on some path from the entry. Registers pushed by the
	 * prologue are saved, never parameters (Watcom does not preserve argument registers).
	 * A call reads its callee's register parameters, so counts are iterated to a fixed point.
	 * Symbol signatures give their own count.
	 */
	private Map<Function, Integer> registerParamCounts(List<Function> order, Set<Function> targets) {
		Map<Function, Integer> n = new HashMap<>();
		for (Function f : order) {
			if (targets.contains(f)) {
				n.put(f, 0);
			}
			else if (!f.isExternal()) {
				int slots = 0;
				for (ghidra.program.model.listing.Parameter p : f.getParameters()) {
					slots += (Math.max(p.getLength(), 1) + 3) / 4;
				}
				n.put(f, f.getCallingConventionName().startsWith(WATCALL) ? Math.min(4, slots) : 0);
			}
		}
		for (int round = 0; round < 8; round++) {
			boolean changed = false;
			for (Function f : order) {
				if (!targets.contains(f) || monitor.isCancelled()) {
					continue;
				}
				int k = liveArgRegisters(f, n);
				if (k != n.get(f)) {
					n.put(f, k);
					changed = true;
				}
			}
			if (!changed) {
				break;
			}
		}
		return n;
	}

	private int liveArgRegisters(Function f, Map<Function, Integer> counts) {
		Instruction in = getInstructionAt(f.getEntryPoint());
		int start = 0xf;
		while (in != null && in.getMnemonicString().equalsIgnoreCase("PUSH") &&
			in.getOpObjects(0).length == 1 && in.getOpObjects(0)[0] instanceof Register) {
			int b = argBit((Register) in.getOpObjects(0)[0]);
			start &= ~b;
			in = in.getNext();
		}
		int live = 0;
		Map<Address, Integer> seen = new HashMap<>();
		Deque<Object[]> work = new ArrayDeque<>();
		work.push(new Object[] { f.getEntryPoint(), start });
		int steps = 0;
		while (!work.isEmpty() && steps++ < 200000) {
			Object[] w = work.pop();
			Address a = (Address) w[0];
			int mask = (Integer) w[1];
			Integer prev = seen.get(a);
			if (prev != null && (prev | mask) == prev) {
				continue; // every undefined register here was already explored
			}
			seen.put(a, prev == null ? mask : prev | mask);
			Instruction i = getInstructionAt(a);
			if (i == null) {
				continue;
			}
			String m = i.getMnemonicString().toUpperCase();
			boolean selfClear = (m.equals("XOR") || m.equals("SUB")) && i.getNumOperands() == 2 &&
				i.getDefaultOperandRepresentation(0).equals(i.getDefaultOperandRepresentation(1));
			if (!selfClear) {
				for (Object o : i.getInputObjects()) {
					if (o instanceof Register) {
						live |= argBit((Register) o) & mask;
					}
				}
			}
			if (m.equals("CALL")) {
				Function callee = calledFunction(i);
				int k = callee == null ? 0 : counts.getOrDefault(callee, 0);
				int reads = (1 << k) - 1;
				live |= reads & mask;
				// The callee clobbers EAX and its argument registers; Windows APIs
				// and other non-Watcom code clobber EAX, ECX and EDX.
				boolean watcom = callee != null && !callee.isExternal() &&
					callee.getCallingConventionName().startsWith(WATCALL);
				mask &= watcom ? ~(reads | 1) : ~(1 | 2 | 8);
			}
			else {
				for (Object o : i.getResultObjects()) {
					if (o instanceof Register) {
						mask &= ~argBit((Register) o);
					}
				}
			}
			if (mask == 0) {
				continue;
			}
			if (m.startsWith("RET")) {
				continue;
			}
			ghidra.program.model.symbol.FlowType flow = i.getFlowType();
			if (i.getFallThrough() != null) {
				work.push(new Object[] { i.getFallThrough(), mask });
			}
			if (flow.isJump()) {
				for (Reference r : i.getReferencesFrom()) {
					if (!r.getReferenceType().isFlow() || r.getReferenceType().isCall()) {
						continue;
					}
					Address t = r.getToAddress();
					if (f.getBody().contains(t)) {
						work.push(new Object[] { t, mask });
					}
					else {
						// Tail call: the target reads its own register parameters.
						Function callee = getFunctionAt(t);
						int k = callee == null ? 0 : counts.getOrDefault(callee, 0);
						live |= ((1 << k) - 1) & mask;
					}
				}
			}
		}
		for (int k = 4; k > 0; k--) {
			if ((live & (1 << (k - 1))) != 0) {
				return k;
			}
		}
		return 0;
	}

	private Function calledFunction(Instruction i) {
		for (Reference r : i.getReferencesFrom()) {
			if (r.getReferenceType().isCall()) {
				Function g = getFunctionAt(r.getToAddress());
				if (g != null && g.isThunk()) {
					g = g.getThunkedFunction(true);
				}
				return g;
			}
		}
		return null;
	}

	/** Bit for EAX (1), EDX (2), EBX (4) or ECX (8); 0 for any other register. */
	private static int argBit(Register r) {
		Register b = r.getBaseRegister();
		int i = ARG_REGS.indexOf(b.getName());
		if (i < 0 && r.getParentRegister() != null) {
			i = ARG_REGS.indexOf(r.getParentRegister().getName());
		}
		return i < 0 ? 0 : 1 << i;
	}

	/**
	 * __watcallN for N register parameters; with stack parameters too, __watcall_sN when
	 * fewer than four registers come first (arguments after a float go on the stack).
	 */
	private static String convention(int regs, int stack) {
		return stack > 0 && regs < 4 ? WATCALL + "_s" + regs : WATCALL + regs;
	}

	private int stackPop(Function f) {
		List<Address> tails = new ArrayList<>();
		ghidra.program.model.listing.InstructionIterator ii =
			currentProgram.getListing().getInstructions(f.getBody(), true);
		while (ii.hasNext()) {
			Instruction in = ii.next();
			int pop = retPop(in);
			if (pop >= 0) {
				return pop;
			}
			// Watcom shares epilogues: a function may end by jumping into another's (Ghidra
			// may type that JMP as a tail call, so look at the mnemonic).
			if (in.getMnemonicString().equalsIgnoreCase("JMP")) {
				for (Address a : in.getFlows()) {
					if (!f.getBody().contains(a)) {
						tails.add(a);
					}
				}
			}
		}
		for (Address a : tails) {
			Instruction in = getInstructionAt(a);
			for (int i = 0; in != null && i < 16; i++, in = in.getNext()) {
				int pop = retPop(in);
				if (pop >= 0) {
					return pop;
				}
				if (in.getFlowType().isJump() || in.getFlowType().isCall()) {
					break;
				}
			}
		}
		return 0;
	}

	/** The bytes a RET pops (0 for a plain RET), or -1 for another instruction. */
	private static int retPop(Instruction in) {
		if (!in.getMnemonicString().equalsIgnoreCase("RET")) {
			return -1;
		}
		if (in.getNumOperands() == 1) {
			Object[] o = in.getOpObjects(0);
			if (o.length == 1 && o[0] instanceof Scalar) {
				return (int) ((Scalar) o[0]).getValue();
			}
		}
		return 0;
	}

	private HighFunction decompile(Function f) {
		DecompileResults r = decomp.decompileFunction(f, 60, monitor);
		return r == null ? null : r.getHighFunction();
	}

	private static boolean hasTag(Function f, String tag) {
		for (FunctionTag t : f.getTags()) {
			if (t.getName().equals(tag)) {
				return true;
			}
		}
		return false;
	}

	/** True when the function reads EAX/EDX/EBX/ECX on entry outside its parameters. */
	private boolean readsArgRegisters(HighFunction hf) {
		Iterator<HighSymbol> it = hf.getLocalSymbolMap().getSymbols();
		while (it.hasNext()) {
			HighSymbol s = it.next();
			String n = s.getName();
			if (!s.isParameter() && n.startsWith("in_") && ARG_REGS.contains(n.substring(3))) {
				return true;
			}
		}
		return false;
	}

	/** Functions ordered so that callees come before their callers. */
	private List<Function> bottomUp() {
		List<Function> order = new ArrayList<>();
		Set<Function> seen = new HashSet<>();
		for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
			Deque<Object[]> stack = new ArrayDeque<>();
			if (seen.add(f)) {
				stack.push(new Object[] { f, f.getCalledFunctions(monitor).iterator() });
			}
			while (!stack.isEmpty()) {
				@SuppressWarnings("unchecked")
				Iterator<Function> it = (Iterator<Function>) stack.peek()[1];
				if (it.hasNext()) {
					Function c = it.next();
					if (seen.add(c)) {
						stack.push(new Object[] { c, c.getCalledFunctions(monitor).iterator() });
					}
				}
				else {
					order.add((Function) stack.pop()[0]);
				}
			}
		}
		return order;
	}
}
