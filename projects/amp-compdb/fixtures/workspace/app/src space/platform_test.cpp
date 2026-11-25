import fixture;

int platform();

int main() { return value() == 42 && platform() == 1 ? 0 : 1; }
