package com.craftinginterpreters.lox;

public class DoubleParser {
    public static double parseDouble(String strDouble) {
        // Raises error message if string is empty or null.
        if (strDouble == null || strDouble.isEmpty()) {
            throw new IllegalArgumentException("Input string is null or empty!");
        }

        double result = 0.0;
        boolean isNegative = false;
        int index = 0;
        int length = strDouble.length();
        boolean fraction = false;
        int fractionalMultiplier = 1;

        // Checks if number is negative.
        if (strDouble.charAt(index) == '-') {
            index++;
            isNegative = true;
        }

        for (; index < length; index++) {
            char currentChar = strDouble.charAt(index);
            // Identifies if decimal portion has begun.
            if (currentChar == '.') {
                fraction = true;
                continue;
            }

            // Check if current character is a number.
            int currentValue = currentChar - '0';
            if (currentValue >= 0 && currentValue <= 9) {
                // Applies operations if we are past or coming towards the decimal.
                if (!fraction) {
                    result = result * 10 + currentValue;
                } else {
                    result += (double) currentValue / (fractionalMultiplier * 10);
                    fractionalMultiplier *= 10;
                }
            } else {
                throw new IllegalArgumentException("Invalid character in input string: " + currentChar);
            }
        }
        // Converts number to negative equivalent.
        return isNegative ? -result: result;

    }

}
