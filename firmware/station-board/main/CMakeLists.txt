# Application entry point. As the main component, it can use every component
# in components/ without listing them in REQUIRES.
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS ".")
