#include <exception>
int main(int argc, char *argv[]) {
  switch (true) {
  case 0 ... 9:
    []() {};
  }
  return 0;
}
