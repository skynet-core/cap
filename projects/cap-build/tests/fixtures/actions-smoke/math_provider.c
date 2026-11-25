extern double cos(double);

int math_value(void) {
  volatile double input = 0.25;
  return cos(input) > 0.0 ? 42 : 0;
}
