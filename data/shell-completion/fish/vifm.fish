# Only 1 hyphen is allowed
function __fish_complete_hyphen
    set -l cmd (commandline -opc)
    set -e cmd[1]
    for i in $cmd
        if test "$i" = "-"
            return
        end
    end
    printf "-"
end

complete -c vifm    -F                            -a "(__fish_complete_hyphen)"
complete -c vifm -r -F        -l "select"                                               -d "Open parent directory of the given path and select specified file in it"
complete -c vifm    -f -s "f"                                                           -d "Make vifm instead of opening files write selection to \$VIFM/vifmfiles and quit"
complete -c vifm -r -F        -l "choose-files"                                         -d "Set output file to write selection into on exit instead of opening files"
complete -c vifm -r -F        -l "choose-dir"                                           -d "Set output file to write last visited directory into on exit"
complete -c vifm -r -F        -l "plugins-dir"                                          -d "Additional plugins directory (can appear multiple times)"
complete -c vifm -r -f        -l "delimiter"                                            -d "Set separator for list of file paths written out by vifm"
complete -c vifm -r -f        -l "on-choose"      -a "(__fish_complete_subcommand)"     -d "Set command to be executed on selected files instead of opening them"
complete -c vifm    -F        -l "logging"                                              -d "Log some operational details. If path is specified, early initialization is logged there"
complete -c vifm -r -f        -l "server-list"                                          -d "List available server names and exit"
complete -c vifm -r -f        -l "server-name"    -a "(vifm --server-list 2>/dev/null)" -d "Name of target or this instance"
complete -c vifm    -f        -l "remote"                                               -d "Pass all arguments that left in command line to vifm server"
complete -c vifm -r -f        -l "remote-expr"                                          -d "Pass expression to vifm server and print result"
complete -c vifm -r -f -s "c"                                                           -d "Run specified Vifm's command on startup"
complete -c vifm -r -f                            -a "+"                                -d "Run specified Vifm's command on startup"
complete -c vifm -r -f -s "h" -l "help"                                                 -d "Show help message and quit"
complete -c vifm -r -f -s "v" -l "version"                                              -d "Show version number and quit"
complete -c vifm    -f        -l "no-configs"                                           -d "Don't read vifmrc and vifminfo"
