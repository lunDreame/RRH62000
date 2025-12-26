idf_component_register(
    SRCS "rrh62000.cpp" "rrh62000_uart_transport.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES driver freertos
)