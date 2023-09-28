# Get full commands from mix
# It should be something like
#   "erl -pa ..."
set -x COMMANDS (ELIXIR_CLI_DRY_RUN=1 mix)

# Only keep the arguments and store them as an array of individual strings
# " -pa ..."
set -x CMD_ARGS (string split -- " " (string sub -s 5 -- $COMMANDS))

# Find the erl script
# The result could be something like
#   - If you/your package manager put erl in /usr/local
#     "/usr/local/bin/erl"
#   - If you installed erlang by asdf
#     "${HOME}/.asdf/shims/erl"
set -x ERL (which erl)

# Either way, lets get the parent dir of the parent dir of the erl binary
#   - "/usr/local"
#   - "${HOME}/.asdf"
set -x ERL_BASE (dirname (dirname $ERL))

# Find the erlexec binary
#   erl is just a shell script that sets up the environment vars
#     and then starts erlexec
set -x ERLEXEC (find $ERL_BASE -name erlexec)

# Set three required environment variables
#   1. BINDIR.
#      The directory that erlexec resides in
set -x BINDIR (dirname $ERLEXEC)

#   2. ROOTDIR.
#      This one is a little bit tricky as there are some difference
#        between the asdf version and others.
#      But it's just the directory where the file start.boot resides in.
#      Note that we might find two start.boot files in ${ERL_BASE},
#        one is used for release while not the other.
#      We need the other one.
set -x START_BOOT (find $ERL_BASE -name start.boot | grep -v release)
set -x ROOTDIR (dirname (dirname $START_BOOT))

#   3. EMU
#      Should just be beam
set -x EMU beam

