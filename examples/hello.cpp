// C++ Hello World for Dunit OS
// Compile with: aarch64-none-elf-g++ -nostdlib -ffreestanding hello.cpp -o
// hello

extern "C" void puts(const char *str);

extern "C" int main() {
  puts("Hello from C++!");
  puts("This is a C++ program running on Dunit OS");
  return 0;
}
