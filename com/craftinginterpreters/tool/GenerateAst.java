package com.craftinginterpreters.tool;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.List;

public class GenerateAst {
    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            System.err.println("Usage: generate_ast <output directory>");
            System.exit(64);
        }
        String outputDirectory = args[0];
        defineAst(outputDirectory, "Expr", Arrays.asList(
            "Binary   : Expr left, Token operator, Expr right",
            "Grouping : Expr expression",
            "Literal  : Object value",
            "Unary    : Token operator, Expr right"
        ));
    }

    private static void defineAst(String outputDirectory, String baseClass, List<String> types) 
    throws IOException{
        String path = outputDirectory + "/" + baseClass + ".java";
        PrintWriter writer = new PrintWriter(path, "UTF-8");

        writer.println("package.com/craftinginterpreters/lox;");
        writer.println();
        writer.println("import java.util.List;");
        writer.println();
        writer.println("abstract class " + baseClass + " {");
        writer.println("}");
        writer.close();
    }
}
