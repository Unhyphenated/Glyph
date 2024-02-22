package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import static com.craftinginterpreters.lox.TokenType.*;

class Scanner {
    private final String source;
    private final List<Token> tokens = new ArrayList<>();
    private int start = 0; // Indicates the start of a line
    private int current = 0; // Indicates the current character
    private int line = 1; // Indicates the current line being scanned

    Scanner(String source) {
        this.source = source;
    }

    List<Token> scanTokens() {
        // Scans each char and appends all tokens to an ArrayList
        while (!isAtEnd()) {
            start = current;
            scanToken();
        }

        tokens.add(new Token(EOF, "", null, line));
        return tokens;
    }

    private void scanToken() {
        char c = advance();
        switch(c) {
            case '(': addToken(LEFT_PAREN); break;
            case ')': addToken(RIGHT_PAREN); break;
            case '{': addToken(LEFT_BRACE); break;
            case '}': addToken(RIGHT_BRACE); break;
            case ',': addToken(COMMA); break;
            case '.': addToken(DOT); break;
            case '-': addToken(MINUS); break;
            case '+': addToken(PLUS); break;
            case '*': addToken(STAR); break;
            case ';': addToken(SEMICOLON); break;
            case '!':
                addToken(match('=') ? BANG_EQUAL: BANG); 
                break;
            case '=':
                addToken(match('=') ? EQUAL_EQUAL: EQUAL); 
                break;
            case '<':
                addToken(match('=') ? LESS_EQUAL: LESS); 
                break;
            case '>':
                addToken(match('=') ? GREATER_EQUAL: GREATER); 
                break;
            case '/':
                if (match('/')) {
                    while(peek() != '\n' && !isAtEnd()) advance();
                } else {
                    addToken(SLASH);
                }
                break;
            case ' ': 
            case '\t': 
            case '\r': 
                break;
            case '\n':
                line++;
                break;

            default: Lox.error(line, "Unexpected character."); break;
        }
    }

    private char advance() {
        return source.charAt(current++);
    }

    private void addToken(TokenType type) {
        addToken(type, null);
    }

    private void addToken(TokenType type, Object literal) {
        String text = source.substring(start, current);
        tokens.add(new Token(type, text, literal, line));
    }

    // Checks if line of code was fully read 
    private boolean isAtEnd() {
        return current >= source.length();
    }

    private boolean match(char expected) {
        // Consumes the current char and moves one char along the line
        if (isAtEnd()) return false;
        if (source.charAt(current++) != expected) return false;
        
        current++;
        return true;
    }

    private char peek() {
        // Checks current char 
        if (isAtEnd()) return '\0';
        return source.charAt(current);
    }
}
