// Headless Ghidra script: give decompiler variables names derived from evidence.
//
//   -postScript AutoNameVariables.java
//
// Only default names are touched (param_N, iVarN, local_XX, ...). Rules, strongest first:
//   1. argument role  a variable passed as argument k to a function whose parameter k has
//                     a real name takes that name (API prototypes, recovered signatures,
//                     and parameters named earlier by this script)
//   2. result source  a variable assigned from a call takes a name derived from the callee
//                     (load_font -> font, fopen -> file)
//   3. struct type    a local whose type is a named struct takes the type's name
// Functions are visited callees-first so parameter names flow up to callers.
//@category A2Racer

import java.util.*;
import java.util.regex.Pattern;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.pcode.*;
import ghidra.program.model.symbol.SourceType;

public class AutoNameVariables extends GhidraScript {

	static final Pattern DEFAULT = Pattern.compile(
		"param_\\d+|[a-z]{1,4}Var\\d+|local_[0-9a-f]+|[a-z]{0,3}Stack_[0-9a-f]+");
	static final Set<String> RESERVED = new HashSet<>(Arrays.asList("int", "char", "long", "short",
		"float", "double", "void", "if", "else", "for", "while", "do", "switch", "case", "return",
		"break", "continue", "default", "goto", "struct", "union", "enum", "static", "const",
		"unsigned", "signed", "sizeof", "typedef", "new", "delete", "this", "register", "auto"));
	static final Map<String, String> RESULT_NAMES = new HashMap<>();
	static {
		String[][] m = { { "fopen", "file" }, { "fsopen", "file" }, { "nmalloc", "buffer" },
			{ "malloc", "buffer" }, { "calloc", "buffer" }, { "realloc", "buffer" },
			{ "strlen", "length" }, { "string_length", "length" }, { "ftell", "position" },
			{ "fread", "count_read" }, { "fwrite", "count_written" }, { "fgetc", "ch" },
			{ "getenv", "env_value" }, { "GetLastError", "error" }, { "strcmp", "cmp" },
			{ "stricmp", "cmp" }, { "strncmp", "cmp" }, { "strnicmp", "cmp" },
			{ "memcmp", "cmp" }, { "strchr", "found" }, { "strrchr", "found" },
			{ "strstr", "found" }, { "rand", "random" }, { "time", "now" } };
		for (String[] e : m) {
			RESULT_NAMES.put(e[0], e[1]);
		}
	}

	private DecompInterface decomp;
	private Set<String> globalNames = new HashSet<>();

	@Override
	protected void run() throws Exception {
		decomp = new DecompInterface();
		decomp.setOptions(new DecompileOptions());
		decomp.toggleSyntaxTree(true);
		decomp.setSimplificationStyle("decompile");
		decomp.openProgram(currentProgram);
		for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
			globalNames.add(f.getName());
		}
		currentProgram.getSymbolTable().getDefinedSymbols()
			.forEachRemaining(s -> globalNames.add(s.getName()));

		int renamed = 0;
		for (Function f : bottomUp()) {
			if (monitor.isCancelled()) {
				break;
			}
			if (f.isThunk() || f.isExternal()) {
				continue;
			}
			renamed += nameFunction(f);
		}
		decomp.dispose();
		println("AutoNameVariables: " + renamed + " variables named");
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

	private int nameFunction(Function f) {
		DecompileResults r = decomp.decompileFunction(f, 60, monitor);
		if (r == null || r.getHighFunction() == null) {
			return 0;
		}
		HighFunction hf = r.getHighFunction();
		Map<HighSymbol, String> plan = new LinkedHashMap<>();
		Set<String> used = new HashSet<>();
		Iterator<HighSymbol> it = hf.getLocalSymbolMap().getSymbols();
		List<HighSymbol> syms = new ArrayList<>();
		while (it.hasNext()) {
			HighSymbol s = it.next();
			syms.add(s);
			used.add(s.getName());
		}
		// Loop counters first: i, j, k, ... in order of where they appear.
		List<HighSymbol> counters = new ArrayList<>();
		for (HighSymbol s : syms) {
			if (DEFAULT.matcher(s.getName()).matches() && !s.isParameter() && isLoopCounter(s)) {
				counters.add(s);
			}
		}
		counters.sort(Comparator.comparing(AutoNameVariables::firstUse));
		String[] letters = { "i", "j", "k", "m", "n" };
		for (int c = 0; c < counters.size() && c < letters.length; c++) {
			String name = unique(letters[c], used);
			used.add(name);
			plan.put(counters.get(c), name);
		}
		for (HighSymbol s : syms) {
			if (!DEFAULT.matcher(s.getName()).matches() || plan.containsKey(s)) {
				continue;
			}
			String name = propose(s);
			if (name == null) {
				continue;
			}
			name = unique(name, used);
			used.add(name);
			plan.put(s, name);
		}
		int n = 0;
		boolean paramsCommitted = false;
		for (Map.Entry<HighSymbol, String> e : plan.entrySet()) {
			HighSymbol s = e.getKey();
			try {
				if (s.isParameter() && !paramsCommitted &&
					f.getSignatureSource() == SourceType.DEFAULT) {
					// Parameters inferred by the decompiler must exist in the database first.
					HighFunctionDBUtil.commitParamsToDatabase(hf, true,
						HighFunctionDBUtil.ReturnCommitOption.NO_COMMIT, SourceType.ANALYSIS);
					paramsCommitted = true;
				}
				HighFunctionDBUtil.updateDBVariable(s, e.getValue(), null, SourceType.ANALYSIS);
				n++;
			}
			catch (Exception ex) {
				// Symbol no longer matches after a commit; it will keep its default name.
			}
		}
		return n;
	}

	private String propose(HighSymbol s) {
		HighVariable hv = s.getHighVariable();
		if (hv == null) {
			return null;
		}
		Map<String, Integer> votes = new HashMap<>();
		Set<String> sources = new HashSet<>();  // callees whose results land in this variable
		boolean otherDefs = false;
		for (Varnode vn : hv.getInstances()) {
			for (Varnode v : withCopies(vn)) {
				Iterator<PcodeOp> d = v.getDescendants();
				while (d.hasNext()) {
					PcodeOp op = d.next();
					if (op.getOpcode() == PcodeOp.PTRADD && op.getInput(1) == v) {
						// Index into an array of a named structure: opponent_data + i -> opponent_index.
						HighVariable base = op.getInput(0).getHigh();
						String sn = base == null ? null : fromType(base.getDataType());
						if (sn != null && !sn.equals("file")) {
							votes.merge(sn + "_index", 2, Integer::sum);
						}
						continue;
					}
					if (op.getOpcode() != PcodeOp.CALL) {
						continue;
					}
					Function callee = getFunctionAt(op.getInput(0).getAddress());
					if (callee == null) {
						continue;
					}
					for (int i = 1; i < op.getNumInputs(); i++) {
						if (op.getInput(i) == v && i - 1 < callee.getParameterCount()) {
							Parameter p = callee.getParameter(i - 1);
							String pn = p == null ? null : fromParamName(p.getName());
							if (pn != null) {
								votes.merge(pn, 1, Integer::sum);
							}
						}
					}
				}
			}
			if (vn.isInput()) {
				otherDefs = true; // a parameter's incoming value: not a call result
			}
			PcodeOp def = vn.getDef();
			while (def != null && (def.getOpcode() == PcodeOp.CAST || def.getOpcode() == PcodeOp.COPY)) {
				def = def.getInput(0).getDef();
			}
			if (def != null && def.getOpcode() == PcodeOp.CALL) {
				Function callee = getFunctionAt(def.getInput(0).getAddress());
				sources.add(callee == null ? "?" : callee.getName());
			}
			else if (def != null && def.getOpcode() != PcodeOp.MULTIEQUAL &&
				def.getOpcode() != PcodeOp.INDIRECT) {
				otherDefs = true;
			}
		}
		// Ghidra often reuses one variable for unrelated values; a variable passed in more than
		// one role (say as a section name and as a file name) keeps its default name.
		if (votes.size() == 1) {
			return votes.keySet().iterator().next();
		}
		if (votes.size() > 1) {
			String idx = votes.keySet().stream().filter(k -> k.endsWith("_index")).findFirst().orElse(null);
			return votes.keySet().stream().filter(k -> k.endsWith("_index")).count() == 1 ? idx : null;
		}
		// A variable that only ever holds one function's result is named after it; one
		// that Ghidra reuses for several unrelated values keeps its default name.
		if (sources.size() == 1 && !otherDefs && !sources.contains("?")) {
			String n = fromCallee(sources.iterator().next());
			if (n != null) {
				return n;
			}
		}
		return fromType(s.getDataType());
	}

	/** A variable incremented or decremented by one and merged around a loop. */
	private boolean isLoopCounter(HighSymbol s) {
		HighVariable hv = s.getHighVariable();
		if (hv == null || hv.getSize() > 4) {
			return false;
		}
		DataType dt = s.getDataType();
		if (dt instanceof Pointer || dt instanceof AbstractFloatDataType) {
			return false;
		}
		Set<Varnode> mine = new HashSet<>(Arrays.asList(hv.getInstances()));
		boolean stepped = false, merged = false;
		for (Varnode vn : hv.getInstances()) {
			PcodeOp def = vn.getDef();
			if (def == null) {
				continue;
			}
			if (def.getOpcode() == PcodeOp.MULTIEQUAL) {
				merged = true;
			}
			if ((def.getOpcode() == PcodeOp.INT_ADD || def.getOpcode() == PcodeOp.INT_SUB) &&
				mine.contains(def.getInput(0)) && def.getInput(1).isConstant()) {
				long c = def.getInput(1).getOffset();
				if (c == 1 || c == 0xffffffffL || c == 0xffffffffffffffffL) {
					stepped = true;
				}
			}
		}
		return stepped && merged;
	}

	private static long firstUse(HighSymbol s) {
		long min = Long.MAX_VALUE;
		HighVariable hv = s.getHighVariable();
		if (hv != null) {
			for (Varnode vn : hv.getInstances()) {
				if (vn.getPCAddress() != null && vn.getPCAddress().getOffset() < min) {
					min = vn.getPCAddress().getOffset();
				}
			}
		}
		return min;
	}

	/** The varnode plus the outputs of CAST/COPY ops reading it (one level). */
	private List<Varnode> withCopies(Varnode vn) {
		List<Varnode> out = new ArrayList<>();
		out.add(vn);
		Iterator<PcodeOp> d = vn.getDescendants();
		while (d.hasNext()) {
			PcodeOp op = d.next();
			if ((op.getOpcode() == PcodeOp.CAST || op.getOpcode() == PcodeOp.COPY) &&
				op.getOutput() != null) {
				out.add(op.getOutput());
			}
		}
		return out;
	}

	/** API-style parameter names: lpFileName -> file_name, hWnd -> wnd, nShowCmd -> show_cmd. */
	static String fromParamName(String n) {
		if (n == null || DEFAULT.matcher(n).matches() || n.startsWith("in_") ||
			n.startsWith("unaff_") || n.startsWith("extraout_")) {
			return null;
		}
		return toSnake(n.replaceFirst(
			"^(lpsz|lpcsz|lpc|lp|psz|pcsz|sz|dw|cb|cch|pv|ppv|h|n|u|b|w|f|l|p|i)(?=[A-Z])", ""));
	}

	static String toSnake(String s) {
		String snake = s.replaceAll("([a-z0-9])([A-Z])", "$1_$2").replaceAll("([A-Z]+)([A-Z][a-z])", "$1_$2")
			.toLowerCase().replaceAll("[^a-z0-9_]+", "_").replaceAll("^_+|_+$", "");
		return snake.isEmpty() || Character.isDigit(snake.charAt(0)) ? null : snake;
	}

	static String fromCallee(String c) {
		if (!c.matches("[A-Za-z_][A-Za-z0-9_]*")) {
			return null;
		}
		if (RESULT_NAMES.containsKey(c)) {
			return RESULT_NAMES.get(c);
		}
		if (c.startsWith("FUN_") || c.startsWith("thunk_")) {
			return null;
		}
		if (c.matches("(is|has|check|can)_.*")) {
			return c;
		}
		String api = c.replaceAll("(ExA|ExW|A|W)$", "");
		if (Character.isUpperCase(c.charAt(0))) {
			api = api.replaceFirst("^(Get|Create|Load|Open|Find|Alloc|Read|Query|Enum|Register)", "");
			return toSnake(api);
		}
		String s = c.replaceFirst("^(xvga_|mdt_|ms_|mm_|mc_|mo_|mns_|mst_|mds_|req[ivw]?_)", "");
		s = s.replaceFirst("^(get|load|read|create|alloc|open|find|make|calc|compute|init|new)_", "");
		return s.equals(c) ? c + "_result" : s;
	}

	static String fromType(DataType dt) {
		boolean pointer = false;
		if (dt instanceof Pointer && ((Pointer) dt).getDataType() != null) {
			dt = ((Pointer) dt).getDataType();
			pointer = true;
		}
		String raw = dt.getName();
		if (raw.equals("FILE") || raw.equals("_iobuf") || raw.equals("__iobuf")) {
			return "file";
		}
		if (pointer && raw.startsWith("IDirect")) {
			return toSnake(raw.substring(1).replaceAll("(A|W)$", ""));
		}
		while (dt instanceof TypeDef) {
			dt = ((TypeDef) dt).getBaseDataType();
		}
		if (!(dt instanceof Structure) && !(dt instanceof Union)) {
			return null;
		}
		String n = raw.replaceFirst("^(tag|_+)", "").replaceAll("(?<=[A-Z])(A|W)$", "")
			.replaceAll("_t$", "");
		return toSnake(n);
	}

	private String unique(String name, Set<String> used) {
		if (RESERVED.contains(name) || globalNames.contains(name)) {
			name = name.endsWith("_index") ? name.replaceAll("_index$", "_idx") : name + "_val";
		}
		String base = name;
		for (int i = 2; used.contains(name); i++) {
			name = base + "_" + i;
		}
		return name;
	}
}
