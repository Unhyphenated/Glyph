package com.craftinginterpreters.lox;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

import static com.craftinginterpreters.lox.Scanner;

public class Lox {
  static boolean hadError = false;
  public static void main(String[] args) throws IOException {
    if (args.length > 1) {
      // Produces error message when several commands are entered when interpreter is ran
      System.out.println("Usage: jlox [script]");
      System.exit(64); 
    } else if (args.length == 1) {
      // Runs a file (usually a file is ran when there is 1 argument)
      runFile(args[0]);
    } else {
      // Prompt is rendered when user input is requested
      runPrompt();
    }
  }

  private static void runFile(String path) throws IOException {
    // Reads and stores all bytes contained within a file
    byte[] bytes = Files.readAllBytes(Paths.get(path));

    // Runs the file by converting bytes into string
    run(new String(bytes, Charset.defaultCharset()));

    // Indicates an error has occurred
    if (hadError) System.exit(65);
  }

  private static void runPrompt() throws IOException {
    InputStreamReader input = new InputStreamReader(System.in);
    BufferedReader reader = new BufferedReader(input);

    for (;;) { // Infinite loop
      System.out.print("> "); // Renders command line
      String line = reader.readLine(); // Awaits user input or command
      if (line == null) break; // Breaks the loop if EOF character is read
      run(line);
      hadError = false;
    }
  }

  private static void run(String source) {
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();

    // For now, just print the tokens.
    for (Token token : tokens) {
      System.out.println(token);
    }
  }

  static void error(int line, String message) {
    report(line, "", message);
  }

  private static void report(int line, String where, String message) {
    System.err.println("[line " + line + "] Error" + where + ": " + message);
    hadError = true;
  }

}