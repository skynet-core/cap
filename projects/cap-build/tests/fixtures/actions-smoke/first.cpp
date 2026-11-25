import action_smoke;

extern "C" int platform_value();

int main() { return module_value() + platform_value() == 42 ? 0 : 1; }
