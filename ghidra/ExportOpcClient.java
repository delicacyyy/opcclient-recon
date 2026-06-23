// Exports complete C-like pseudocode and evidence maps for OPCClient.exe.
// Run by analyzeHeadless as a post-script.
//@category OPCClient

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ExternalLocation;
import ghidra.program.model.symbol.ExternalLocationIterator;
import ghidra.program.model.symbol.ExternalManager;
import ghidra.program.util.DefinedDataIterator;
import ghidra.util.task.ConsoleTaskMonitor;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

public class ExportOpcClient extends GhidraScript {
    private static final int DECOMPILE_TIMEOUT_SECONDS = 120;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            throw new IllegalArgumentException("Usage: ExportOpcClient.java <artifact-root> [annotation-csv]");
        }

        Path artifactRoot = Paths.get(args[0]).toAbsolutePath().normalize();
        Path rawDir = artifactRoot.resolve("decompiled").resolve("raw");
        Path annotatedDir = artifactRoot.resolve("decompiled").resolve("annotated");
        Path mapsDir = artifactRoot.resolve("maps");
        Files.createDirectories(rawDir);
        Files.createDirectories(annotatedDir);
        Files.createDirectories(mapsDir);

        Path annotationCsv = args.length >= 2 ? Paths.get(args[1]).toAbsolutePath().normalize() : null;
        List<String[]> annotations = loadAnnotations(annotationCsv);

        DecompInterface decompiler = new DecompInterface();
        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(true);
        decompiler.setSimplificationStyle("decompile");
        if (!decompiler.openProgram(currentProgram)) {
            throw new IOException("Ghidra decompiler could not open the program");
        }

        Path combinedRaw = rawDir.resolve("OPCClient_all.c");
        Path combinedAnnotated = annotatedDir.resolve("OPCClient_all_annotated.c");
        Path functionsCsv = mapsDir.resolve("functions.csv");
        Path callsCsv = mapsDir.resolve("callgraph.csv");
        Path stringsCsv = mapsDir.resolve("strings.csv");
        Path importsCsv = mapsDir.resolve("imports.csv");
        Path unresolvedTxt = mapsDir.resolve("unresolved-functions.txt");
        Path summaryMd = mapsDir.resolve("analysis-summary.md");

        List<String> unresolved = new ArrayList<>();
        int functionCount = 0;
        int decompiledCount = 0;
        int externalCount = 0;

        try (
            BufferedWriter rawAll = writer(combinedRaw);
            BufferedWriter annotatedAll = writer(combinedAnnotated);
            BufferedWriter functions = writer(functionsCsv);
            BufferedWriter calls = writer(callsCsv);
            BufferedWriter imports = writer(importsCsv)
        ) {
            functions.write("address,name,namespace,size,external,thunk,decompiled,status\n");
            calls.write("caller_address,caller_name,callee_address,callee_name\n");
            imports.write("address,name,library,original_name,is_function\n");

            ExternalManager externalManager = currentProgram.getExternalManager();
            for (String library : externalManager.getExternalLibraryNames()) {
                ExternalLocationIterator locations = externalManager.getExternalLocations(library);
                while (locations.hasNext()) {
                    ExternalLocation location = locations.next();
                    externalCount++;
                    Address externalAddress = location.getExternalSpaceAddress();
                    imports.write(csv(
                        externalAddress == null ? "" : externalAddress.toString(),
                        location.getLabel(),
                        library,
                        location.getOriginalImportedName(),
                        Boolean.toString(location.isFunction())
                    ));
                    imports.write("\n");
                }
            }

            FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
            while (iterator.hasNext() && !monitor.isCancelled()) {
                Function function = iterator.next();
                functionCount++;
                Address entry = function.getEntryPoint();
                String address = entry.toString();
                String name = function.getName();
                String namespace = function.getParentNamespace().getName(true);
                boolean external = function.isExternal();
                boolean thunk = function.isThunk();
                long size = function.getBody().getNumAddresses();

                Set<String> evidence = collectEvidence(function);
                String suggestedName = suggestedName(function, evidence, annotations);
                String confidence = confidence(function, suggestedName, evidence, annotations);

                boolean success = false;
                String status;
                String cCode;
                if (external) {
                    status = "external";
                    cCode = "/* External/imported function: " + name + " */\n";
                } else {
                    DecompileResults result = decompiler.decompileFunction(
                        function,
                        DECOMPILE_TIMEOUT_SECONDS,
                        new ConsoleTaskMonitor()
                    );
                    success = result.decompileCompleted() && result.getDecompiledFunction() != null;
                    if (success) {
                        decompiledCount++;
                        status = "ok";
                        cCode = result.getDecompiledFunction().getC();
                    } else {
                        status = result.getErrorMessage();
                        if (status == null || status.isBlank()) {
                            status = "decompilation failed";
                        }
                        unresolved.add(address + " " + name + ": " + status);
                        cCode = "/* DECOMPILATION FAILED: " + status.replace("*/", "* /") + " */\n";
                    }
                }

                String safeName = safeFilename(address + "_" + name) + ".c";
                Path rawFile = rawDir.resolve(safeName);
                Path annotatedFile = annotatedDir.resolve(safeName);

                String rawHeader =
                    "/*\n" +
                    " * Address: " + address + "\n" +
                    " * Ghidra name: " + name + "\n" +
                    " * Namespace: " + namespace + "\n" +
                    " * Decompiled source is an approximation, not original source code.\n" +
                    " */\n\n";
                Files.writeString(rawFile, rawHeader + cCode, StandardCharsets.UTF_8);

                String annotatedHeader =
                    "/*\n" +
                    " * Address: " + address + "\n" +
                    " * Ghidra name: " + name + "\n" +
                    " * Suggested semantic name: " + suggestedName + "\n" +
                    " * Confidence: " + confidence + "\n" +
                    " * Evidence: " + (evidence.isEmpty() ? "(none)" : String.join(" | ", evidence)) + "\n" +
                    " * This annotation records inference; it does not claim recovery of original names.\n" +
                    " */\n\n";
                Files.writeString(annotatedFile, annotatedHeader + cCode, StandardCharsets.UTF_8);

                rawAll.write(rawHeader);
                rawAll.write(cCode);
                rawAll.write("\n\n");
                annotatedAll.write(annotatedHeader);
                annotatedAll.write(cCode);
                annotatedAll.write("\n\n");

                functions.write(csv(
                    address,
                    name,
                    namespace,
                    Long.toString(size),
                    Boolean.toString(external),
                    Boolean.toString(thunk),
                    Boolean.toString(success),
                    status
                ));
                functions.write("\n");

                for (Function called : function.getCalledFunctions(monitor)) {
                    calls.write(csv(
                        address,
                        name,
                        called.getEntryPoint().toString(),
                        called.getName()
                    ));
                    calls.write("\n");
                }
            }
        } finally {
            decompiler.dispose();
        }

        exportStrings(stringsCsv);
        Files.write(unresolvedTxt, unresolved, StandardCharsets.UTF_8);
        exportProgramMetadata(mapsDir.resolve("program-metadata.txt"));

        String summary =
            "# Ghidra export summary\n\n" +
            "- Program: `" + currentProgram.getName() + "`\n" +
            "- Language: `" + currentProgram.getLanguageID() + "`\n" +
            "- Compiler specification: `" + currentProgram.getCompilerSpec().getCompilerSpecID() + "`\n" +
            "- Image base: `" + currentProgram.getImageBase() + "`\n" +
            "- Functions discovered: " + functionCount + "\n" +
            "- Functions decompiled: " + decompiledCount + "\n" +
            "- External/import functions: " + externalCount + "\n" +
            "- Decompilation failures: " + unresolved.size() + "\n\n" +
            "Generated pseudocode is an analytical approximation and is not the original FactorySoft source.\n";
        Files.writeString(summaryMd, summary, StandardCharsets.UTF_8);

        // Make a stable copy of the exact annotations input used for this run.
        if (annotationCsv != null && Files.exists(annotationCsv)) {
            Files.copy(annotationCsv, mapsDir.resolve("annotations-used.csv"), StandardCopyOption.REPLACE_EXISTING);
        }
        println("Exported OPCClient artifacts to " + artifactRoot);
    }

    private void exportStrings(Path output) throws IOException {
        try (BufferedWriter strings = writer(output)) {
            strings.write("address,value,references_from\n");
            for (Data data : DefinedDataIterator.byDataInstance(currentProgram, Data::hasStringValue)) {
                StringDataInstance instance = StringDataInstance.getStringDataInstance(data);
                String value = instance.getStringValue();
                if (value == null) {
                    value = data.getDefaultValueRepresentation();
                }
                List<String> refs = new ArrayList<>();
                ReferenceIterator iterator = currentProgram.getReferenceManager().getReferencesTo(data.getAddress());
                while (iterator.hasNext()) {
                    refs.add(iterator.next().getFromAddress().toString());
                }
                strings.write(csv(data.getAddress().toString(), value, String.join(";", refs)));
                strings.write("\n");
            }
        }
    }

    private void exportProgramMetadata(Path output) throws IOException {
        List<String> lines = new ArrayList<>();
        lines.add("name=" + currentProgram.getName());
        lines.add("executable_path=" + currentProgram.getExecutablePath());
        lines.add("executable_format=" + currentProgram.getExecutableFormat());
        lines.add("language_id=" + currentProgram.getLanguageID());
        lines.add("compiler_spec=" + currentProgram.getCompilerSpec().getCompilerSpecID());
        lines.add("image_base=" + currentProgram.getImageBase());
        lines.add("min_address=" + currentProgram.getMinAddress());
        lines.add("max_address=" + currentProgram.getMaxAddress());
        Files.write(output, lines, StandardCharsets.UTF_8);
    }

    private Set<String> collectEvidence(Function function) {
        Set<String> evidence = new LinkedHashSet<>();
        if (function.isExternal()) {
            evidence.add("import:" + function.getName());
            return evidence;
        }

        InstructionIterator instructions = currentProgram.getListing().getInstructions(function.getBody(), true);
        while (instructions.hasNext() && evidence.size() < 24) {
            Instruction instruction = instructions.next();
            for (Reference reference : instruction.getReferencesFrom()) {
                Address target = reference.getToAddress();
                Function called = currentProgram.getFunctionManager().getFunctionAt(target);
                if (called != null && called.isExternal()) {
                    evidence.add("call:" + called.getName());
                }
                Data data = currentProgram.getListing().getDataAt(target);
                if (data != null && data.hasStringValue()) {
                    String value = StringDataInstance.getStringDataInstance(data).getStringValue();
                    if (value != null && !value.isBlank()) {
                        value = value.replace("\r", "\\r").replace("\n", "\\n");
                        if (value.length() > 100) {
                            value = value.substring(0, 100) + "...";
                        }
                        evidence.add("string:" + value);
                    }
                }
            }
        }
        return evidence;
    }

    private String suggestedName(Function function, Set<String> evidence, List<String[]> annotations) {
        String address = function.getEntryPoint().toString().toUpperCase(Locale.ROOT);
        for (String[] row : annotations) {
            if (row.length >= 2 && row[0].equalsIgnoreCase(address)) {
                return row[1];
            }
        }

        String text = String.join(" ", evidence).toLowerCase(Locale.ROOT);
        if (text.contains("cocreateinstanceex") || text.contains("error connecting to opc server")) {
            return "ConnectToOpcServer";
        }
        if (text.contains("opc 2.0 server browser") || text.contains("available servers")) {
            return "EnumerateOpcServers";
        }
        if (text.contains("addgroup")) {
            return "CreateOpcGroup";
        }
        if (text.contains("additems")) {
            return "AddOpcItems";
        }
        if (text.contains("sync read")) {
            return "PerformSynchronousRead";
        }
        if (text.contains("async read") || text.contains("refresh2")) {
            return "PerformAsynchronousReadOrRefresh";
        }
        if (text.contains("sync write")) {
            return "PerformSynchronousWrite";
        }
        if (text.contains("async write")) {
            return "PerformAsynchronousWrite";
        }
        if (text.contains("item properties")) {
            return "DisplayItemProperties";
        }
        if (text.contains("server status")) {
            return "DisplayServerStatus";
        }
        if (text.contains("regconnectregistry") || text.contains("hkey")) {
            return "InspectRemoteRegistryForOpcServers";
        }
        if (text.contains("wnetenumresource")) {
            return "EnumerateNetworkComputers";
        }
        return function.getName();
    }

    private String confidence(Function function, String suggestedName, Set<String> evidence, List<String[]> annotations) {
        String address = function.getEntryPoint().toString().toUpperCase(Locale.ROOT);
        for (String[] row : annotations) {
            if (row.length >= 3 && row[0].equalsIgnoreCase(address)) {
                return row[2];
            }
        }
        if (!suggestedName.equals(function.getName()) && evidence.size() >= 2) {
            return "medium";
        }
        if (!suggestedName.equals(function.getName())) {
            return "low";
        }
        return "unclassified";
    }

    private List<String[]> loadAnnotations(Path path) throws IOException {
        if (path == null || !Files.exists(path)) {
            return Collections.emptyList();
        }
        List<String[]> rows = new ArrayList<>();
        List<String> lines = Files.readAllLines(path, StandardCharsets.UTF_8);
        for (int i = 0; i < lines.size(); i++) {
            String line = lines.get(i).trim();
            if (line.isEmpty() || line.startsWith("#") || (i == 0 && line.toLowerCase(Locale.ROOT).startsWith("address,"))) {
                continue;
            }
            rows.add(line.split(",", 4));
        }
        return rows;
    }

    private static BufferedWriter writer(Path path) throws IOException {
        Files.createDirectories(path.getParent());
        return new BufferedWriter(
            new OutputStreamWriter(new FileOutputStream(path.toFile()), StandardCharsets.UTF_8)
        );
    }

    private static String safeFilename(String value) {
        return value.replaceAll("[^A-Za-z0-9._-]", "_");
    }

    private static String csv(String... values) {
        List<String> escaped = new ArrayList<>();
        for (String value : values) {
            if (value == null) {
                value = "";
            }
            escaped.add("\"" + value.replace("\"", "\"\"") + "\"");
        }
        return String.join(",", escaped);
    }
}
