package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import static com.craftinginterpreters.lox.TokenType.*;
import static com.craftinginterpreters.lox.DoubleParser.*;

class Scanner {
    // Hash map of reserved keywords.
    private static final Map<String, TokenType> keywords;
    static {
        keywords = new HashMap<>();
        keywords.put("and", AND);
        keywords.put("or", OR);
        keywords.put("class", CLASS);
        keywords.put("else", ELSE);
        keywords.put("if", IF);
        keywords.put("false", FALSE);
        keywords.put("true", TRUE);
        keywords.put("nil", NIL);
        keywords.put("while", WHILE);
        keywords.put("super", SUPER);
        keywords.put("print", RETURN);
        keywords.put("this", THIS);
        keywords.put("var", VAR);
        keywords.put("for", FOR);
        keywords.put("fun", FUN);
    }
    private final String source;
    private final List<Token> tokens = new ArrayList<>();
    private int start = 0; // Indicates the start of a line.
    private int current = 0; // Indicates the current character.
    private int line = 1; // Indicates the current line being scanned.

    Scanner(String source) {
        this.source = source;
    }

    List<Token> scanTokens() {
        // Scans each char and appends all tokens to an ArrayList.
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
            case '"':
                string();
            default: 
                if (isDigit(c)) {
                    number();
                } else if (isAlpha(c)) {
                    identifier();
                } else {
                    Lox.error(line, "Unexpected character."); 
                }
                break;
        }
    }

    private void identifier() {
        while(isAlphaNumeric(peek())) advance();

        String word = source.substring(start, current);
        TokenType type = keywords.get(word);
        type = type == null ? IDENTIFIER : type;
        addToken(type);
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

    private void string() {
        // Traverses all characters past the first '"' scanning each character until the end
        // of the program is reached.
        while (peek() != '"' && !isAtEnd()) {
            // If a line break occurs, move to the next one
            if (peek() == '\n') line++;
            // Consume each character and move onto the next one
            advance();
        }

        // If the second '"' is not found, raise an error.
        if (isAtEnd()) {
            Lox.error(line, "Undetermined string.");
        }
        // Consume the closing '"'.
        advance();

        // Initialise the string, avoiding the first '"', (hence start + 1)
        // and the second '"', (hence current - 1).
        String value = source.substring(start + 1, current - 1);

        addToken(STRING, value);
    }

    private boolean isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    private void number() {
        // Keep advancing if a digit is discovered.
        while (isDigit(peek())) advance();

        // If a decimal point is found, check if the next character is a number.
        // If it is, continue. Otherwise, extract the number before the decimal point.
        if (peek() == '.' && isDigit(peekNext())) {
            advance();
            while (isDigit(peek())) advance();
        }

        // Convert the string to double.
        double value = parseDouble(source.substring(start, current));
        addToken(NUMBER, value);
    }

    private char peekNext() {
        // While peek merely scans the current character without consuming it,
        // peekNext scans the character right after.
        if (current + 1 >= source.length()) return '\0';
        return source.charAt(current + 1);
    }

    private boolean isAlpha(char c) {
        return (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_');
    }

    private boolean isAlphaNumeric(char c) {
        return isAlpha(c) || isDigit(c);
    }
}
