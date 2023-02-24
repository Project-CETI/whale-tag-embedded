int gpio_export(const char * gpio_number);
int gpio_unexport(const char * gpio_number);
int gpio_set_direction(const char * gpio_direction, const char * in_or_out);
int gpio_set_value(const char * gpio_value, const char * gpio_1_or_0);
int gpio_get_value(const char * gpio_value);