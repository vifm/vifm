#!/usr/bin/env zsh

# The MIT License (MIT)
#
# Copyright (c) 2015 Vincent Breitmoser
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

zmodload zsh/zpty || { echo 'error: missing module zsh/zpty' >&2; exit 1 }

# spawn shell
zpty z zsh -f -i

# line buffer for pty output
local line

setopt rcquotes
() {
    zpty -w z source $1
    repeat 4; do
        zpty -r z line
        [[ $line == ok* ]] && return
    done
    echo 'error initializing.' >&2
    exit 2
} =( <<< '
# no prompt!
PROMPT=

# load completion system
autoload compinit
compinit -d ${ZSH_CAPTURE_COMPDUMP:-${ZDOTDIR:-$HOME}/.zcompdump}

# never run a command
bindkey ''^M'' undefined
bindkey ''^J'' undefined
bindkey ''^I'' complete-word

# send a line with null-byte at the end before and after completions are output
null-line () {
    echo -E - $''\0''
}
compprefuncs=( null-line )
comppostfuncs=( null-line exit )

# never group stuff!
zstyle '':completion:*'' list-grouped false
# don''t insert tab when attempting completion on empty line
zstyle '':completion:*'' insert-tab false
# no list separator, this saves some stripping later on
zstyle '':completion:*'' list-separator ''''

# we use zparseopts
zmodload zsh/zutil

# override compadd (this our hook)
compadd () {

    # check if any of -O, -A or -D are given
    if [[ ${@[1,(i)(-|--)]} == *-(O|A|D)\ * ]]; then
        # if that is the case, just delegate and leave
        builtin compadd "$@"
        return $?
    fi

    # ok, this concerns us!
    # echo -E - got this: "$@"

    # be careful with namespacing here, we don''t want to mess with stuff that
    # should be passed to compadd!
    typeset -a __hits __dscr __tmp

    # do we have a description parameter?
    # note we don''t use zparseopts here because of combined option parameters
    # with arguments like -default- confuse it.
    if (( $@[(I)-d] )); then # kind of a hack, $+@[(r)-d] doesn''t work because of line noise overload
        # next param after -d
        __tmp=${@[$[${@[(i)-d]}+1]]}
        # description can be given as an array parameter name, or inline () array
        if [[ $__tmp == \(* ]]; then
            eval "__dscr=$__tmp"
        else
            __dscr=( "${(@P)__tmp}" )
        fi
    fi

    # capture completions by injecting -A parameter into the compadd call.
    # this takes care of matching for us.
    builtin compadd -A __hits -D __dscr "$@"

    # JESUS CHRIST IT TOOK ME FOREVER TO FIGURE OUT THIS OPTION WAS SET AND WAS MESSING WITH MY SHIT HERE
    setopt localoptions norcexpandparam extendedglob

    # extract prefixes and suffixes from compadd call. we can''t do zsh''s cool
    # -r remove-func magic, but it''s better than nothing.
    typeset -A apre hpre hsuf asuf
    zparseopts -E P:=apre p:=hpre S:=asuf s:=hsuf

    # append / to directories? we are only emulating -f in a half-assed way
    # here, but it''s better than nothing.
    integer dirsuf=0
    # don''t be fooled by -default- >.>
    if [[ -z $hsuf && "${${@//-default-/}% -# *}" == *-[[:alnum:]]#f* ]]; then
        dirsuf=1
    fi

    # just drop
    [[ -n $__hits ]] || return

    # this is the point where we have all matches in $__hits and all
    # descriptions in $__dscr!

    # display all matches
    local dsuf dscr
    for i in {1..$#__hits}; do

        # add a dir suffix?
        (( dirsuf )) && [[ -d $__hits[$i] ]] && dsuf=/ || dsuf=
        # description to be displayed afterwards
        (( $#__dscr >= $i )) && dscr=" -- ${${__dscr[$i]}##$__hits[$i] #}" || dscr=

        echo -E - $IPREFIX$apre$hpre$__hits[$i]$dsuf$hsuf$asuf$dscr

    done

}

# signal success!
echo ok')

zpty -w z "$*"$'\t'

integer tog=0
# read from the pty, and parse linewise
while zpty -r z; do :; done | while IFS= read -r line; do
    if [[ $line == *$'\0\r' ]]; then
        (( tog++ )) && return 0 || continue
    fi
    # display between toggles
    (( tog )) && echo -E - $line
done

return 2
