function(
    disable_iwyu # 声明了一个接收target参数的函数，用于禁用指定目标的"IWYU"（Include What You Use）静态分析工具
    target)

    set_property(
        TARGET ${target}
        PROPERTY CXX_INCLUDE_WHAT_YOU_USE
                 "")
    set_property(
        TARGET ${target}
        PROPERTY C_INCLUDE_WHAT_YOU_USE
                 "")
endfunction()
