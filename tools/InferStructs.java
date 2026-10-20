// Headless Ghidra script: fill in game structure layouts from how the code uses them.
//
//   -postScript InferStructs.java <out.tsv>
//
// Every variable typed as a pointer to a structure in /A2Racer (the game types named by
// A2MENU.DAT's debug info, and those in symbols/structs.tsv) is run through Ghidra's
// structure fill-out, which adds a member for each load and store through it at an
// offset no field covers yet; those become field_XX. An array stride (p + i * 0x40)
// sets the structure's size. The added fields are written to <out.tsv>
// in the format of symbols/structs.tsv; decompile_all.sh merges them across modules into
// symbols/structs_auto.tsv, which ApplyWatcom.java loads below the hand-written layouts.
//@category A2Racer

import java.io.File;
import java.io.PrintWriter;
import java.util.*;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.util.FillOutStructureHelper;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.*;

public class InferStructs extends GhidraScript {

	static final CategoryPath GAME = new CategoryPath("/A2Racer");

	/** Structures filled out, and the largest array stride seen per structure. */
	private final Set<String> seen = new TreeSet<>();
	private final Map<String, Long> stride = new HashMap<>();

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		DecompInterface decomp = new DecompInterface();
		decomp.setOptions(new DecompileOptions());
		decomp.toggleSyntaxTree(true);
		decomp.setSimplificationStyle("decompile");
		decomp.openProgram(currentProgram);
		FillOutStructureHelper helper = new FillOutStructureHelper(currentProgram, monitor);

		for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
			if (monitor.isCancelled() || f.isThunk() || f.isExternal()) {
				continue;
			}
			DecompileResults r = decomp.decompileFunction(f, 60, monitor);
			if (r == null || r.getHighFunction() == null) {
				continue;
			}
			HighFunction hf = r.getHighFunction();
			Set<HighVariable> vars = new LinkedHashSet<>();
			Iterator<PcodeOpAST> ops = hf.getPcodeOps();
			while (ops.hasNext()) {
				PcodeOpAST op = ops.next();
				for (Varnode v : op.getInputs()) {
					if (v.getHigh() != null && gameStruct(v.getHigh().getDataType()) != null) {
						vars.add(v.getHigh());
					}
				}
				if (op.getOpcode() == PcodeOp.PTRADD && op.getNumInputs() == 3 &&
					op.getInput(2).isConstant()) {
					Structure s = gameStruct(op.getInput(0).getHigh() == null ? null
						: op.getInput(0).getHigh().getDataType());
					if (s != null) {
						stride.merge(s.getName(), op.getInput(2).getOffset(), Math::max);
					}
				}
			}
			for (HighVariable hv : vars) {
				Structure s = gameStruct(hv.getDataType());
				if (s == null) {
					continue;
				}
				try {
					helper.processStructure(hv, f, false, false, decomp);
				}
				catch (Exception e) {
					continue;
				}
				seen.add(s.getName());
			}
		}
		decomp.dispose();

		// The fill-out adds members to the structure itself, unnamed; name them by offset.
		int added = 0;
		DataTypeManager dtm = currentProgram.getDataTypeManager();
		Iterator<Structure> structs = dtm.getAllStructures();
		while (structs.hasNext()) {
			Structure s = structs.next();
			if (!GAME.equals(s.getCategoryPath())) {
				continue;
			}
			for (DataTypeComponent c : s.getDefinedComponents()) {
				if (c.getFieldName() == null) {
					c.setFieldName(String.format("field_%02x", c.getOffset()));
					added++;
				}
			}
		}
		for (Map.Entry<String, Long> e : stride.entrySet()) {
			Structure s = (Structure) dtm.getDataType(GAME, e.getKey());
			if (s != null && e.getValue() > s.getLength() && e.getValue() < 0x10000) {
				s.growStructure((int) (e.getValue() - s.getLength()));
			}
		}

		// Write every inferred (field_XX) member and each size, so the merged file keeps
		// fields added by earlier runs as well.
		try (PrintWriter out = new PrintWriter(new File(args[0]), "UTF-8")) {
			out.println("struct\toffset\ttype\tname\tcomment");
			Iterator<Structure> all = dtm.getAllStructures();
			List<Structure> game = new ArrayList<>();
			while (all.hasNext()) {
				Structure s = all.next();
				if (GAME.equals(s.getCategoryPath())) {
					game.add(s);
				}
			}
			game.sort(Comparator.comparing(Structure::getName));
			for (Structure s : game) {
				for (DataTypeComponent c : s.getDefinedComponents()) {
					String n = c.getFieldName();
					if (n != null && n.matches("field_[0-9a-f]+")) {
						out.println(s.getName() + "\t" + Integer.toHexString(c.getOffset()) + "\t" +
							typeName(c.getDataType()) + "\t" + n + "\t");
					}
				}
				if (!s.isZeroLength() && s.getLength() > 0) {
					out.println(s.getName() + "\t" + Integer.toHexString(s.getLength()) + "\t-\t__size\t");
				}
			}
		}
		println("InferStructs: " + added + " fields added to " + seen.size() + " structures -> " +
			args[0]);
	}

	/** The /A2Racer structure a pointer type points to, or null. */
	private static Structure gameStruct(DataType dt) {
		while (dt instanceof TypeDef) {
			dt = ((TypeDef) dt).getBaseDataType();
		}
		if (!(dt instanceof Pointer)) {
			return null;
		}
		DataType p = ((Pointer) dt).getDataType();
		while (p instanceof TypeDef) {
			p = ((TypeDef) p).getBaseDataType();
		}
		if (p instanceof Structure && GAME.equals(p.getCategoryPath())) {
			return (Structure) p;
		}
		return null;
	}

	/** The type in symbols/structs.tsv notation ("int", "frame_t*", "undefined4"). */
	private static String typeName(DataType dt) {
		if (dt instanceof Pointer) {
			DataType p = ((Pointer) dt).getDataType();
			if (p == null || p instanceof Undefined || p instanceof DefaultDataType ||
				p instanceof VoidDataType) {
				return "void*";
			}
			return typeName(p) + "*";
		}
		String n = dt.getName();
		switch (n) {
			case "uint": case "int": case "short": case "ushort": case "char": case "uchar":
			case "float": case "double": case "long": case "ulong": case "bool":
				return n;
			case "byte": return "uchar";
			case "word": return "ushort";
			case "dword": return "uint";
		}
		return n;
	}
}
