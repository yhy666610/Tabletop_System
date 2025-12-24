void logger_init(void);
void shell_init(void);

void component_init(void)
{
    logger_init();
    shell_init();
}
