#include <vector>
#include <stdlib.h>

class Abuf {
    public:
        std::vector<char> buffer;
        int len;
        Abuf() = default;

        void append(const char* s, int len) {
            if (s == nullptr || len <= 0) return;
            buffer.insert(buffer.end(), s, s + len);
        }

        size_t size() const {
            return buffer.size();
        }

        const char* data() const {
            return buffer.data();
        }

        // Destructor - vector handles its own memory cleanup
        ~Abuf() = default;
};