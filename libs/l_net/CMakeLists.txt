
set(L_NETLIST
    l_net.c
    l_net.h
)
if(WIN32)
    set(L_NETLIST ${L_NETLIST} l_net_wins.c l_net_wins.h)
else()
    set(L_NETLIST ${L_NETLIST} l_net_berkley.c)
endif()
add_library(l_net ${L_NETLIST})
if(WIN32)
    target_link_libraries(l_net ws2_32)
endif()
