public class DoubleParser {
    public static double parseDouble(String strDouble) {
        // Raises error message if string is empty or null
        if (strDouble == null || strDouble.isEmpty()) {
            throw new IllegalArgumentException("Input string is null or empty!");
        }
        
    }
}
