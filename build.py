#!python3.6

"""
Wrapper for build.sh.
"""

# TODO: reqquire -j

import argparse
import datetime
import logging
import os
import subprocess
import sys
from typing import List, Optional, Tuple


logger = logging.getLogger(__name__)


def main(m: str, T: str, O: str, D: str, R: str, X: str,
         pairs_var_val: List[Tuple[str, Optional[str]]],
         no_u: bool, no_U: bool, no_x: bool,
         no_mk_crossgdb_yes: bool,
         no_mk_debug_yes: bool,
         # no_mk_doc_no: bool,
         # no_mk_html_no: bool,
         # no_mk_info_no: bool,
         no_mk_kdebug_yes: bool,
         # no_mk_lint_yes: bool,
         # no_mk_man_no: bool,
         # no_mk_share_no: bool,
         path_log: str,
         opts_other: List[str],
         target: str, build_info: str):
    if False:
        dir_top = os.path.dirname(os.path.abspath(__file__))
        os.chdir(dir_top)

    # if not exist, tails when write to path_log
    subprocess.call(['mkdir', '-p', O])

    args_mk = []
    # @formatter:off
    if not no_u:               args_mk.append('-u')
    if not no_U:               args_mk.append('-U')
    if not no_x:               args_mk.append('-x')
    if not no_mk_crossgdb_yes: args_mk.append('-VMKCROSSGDB=yes')
    if not no_mk_debug_yes:    args_mk.append('-VMKDEBUG=yes')
    # if not no_mk_doc_no:       args_mk.append('-VMKDOC=no')
    # if not no_mk_html_no:      args_mk.append('-VMKHTML=no')
    # if not no_mk_info_no:      args_mk.append('-VMKINFO=no')
    if not no_mk_kdebug_yes:   args_mk.append('-VMKKDEBUG=yes')
    # if not no_mk_lint_yes:     args_mk.append('-VMKLINT=yes')
    # if not no_mk_man_no:       args_mk.append('-VMKMAN=no')
    # if not no_mk_share_no:     args_mk.append('-VMKSHARE=no')
    # @formatter:on

    execs = [
                 './build.sh',
                 '-m', m,
                 '-T', T, '-O', O, '-D', D, '-R', R, '-X', X,
                 *args_mk,
                 f'-VBUILDINFO={build_info}',
                 *opts_other,
                 target
             ]
    logger.info(f'execute: {execs}')

    # https://stackoverflow.com/questions/15535240/python-popen-write-to-stdout-and-log-file-simultaneously
    ps = subprocess.Popen(execs,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    with open(path_log, 'bw') as f:
        for line in ps.stdout:
            sys.stdout.buffer.write(line)
            f.write(line)
    logger.debug('exit')  # TODO: exit with pipe status


if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)

    parser = argparse.ArgumentParser(
        description='build.sh wrapper',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        epilog='Options not list here are also passed to build.sh (i.g. -j4).'
    )

    # @formatter:off
    parser.add_argument('-m', metavar='mach',    default='amd64',          help='help_m')
    parser.add_argument('-T', metavar='tools',   default='../tools/', help='help_T')
    parser.add_argument('-O', metavar='obj',     default='../obj.${mach}/',  help='help_O')
    parser.add_argument('-D', metavar='dest',    default='../dest.${mach}/', help='help_D')
    parser.add_argument('-R', metavar='release', default='../rel.${mach}/',  help='help_R')
    parser.add_argument('-X', metavar='x11src',  default='../xsrc/',      help='help_X')

    parser.add_argument('-V', metavar='var=[value]', action='append', help='help_V')

    parser.add_argument('--no-u'              , action='store_true', help='without -u')
    parser.add_argument('--no-U'              , action='store_true', help='without -U')
    parser.add_argument('--no-x'              , action='store_true', help='without -x')
    parser.add_argument('--no-mk-crossgdb-yes', action='store_true', help='without -VMKCROSSGDB=yes')
    parser.add_argument('--no-mk-debug-yes',    action='store_true', help='without -VMKDEBUG=yes')
    # parser.add_argument('--no-mk-doc-no',       action='store_true', help='without -VMKDOC=no')
    # parser.add_argument('--no-mk-html-no',      action='store_true', help='without -VMKHTML=no')
    # parser.add_argument('--no-mk-info-no',      action='store_true', help='without -VMKINFO=no')
    parser.add_argument('--no-mk-kdebug-yes',   action='store_true', help='without -VMKKDEBUG=yes')
    # parser.add_argument('--no-mk-lint-yes',     action='store_true', help='without -VMKLINT=yes')  # no?
    # parser.add_argument('--no-mk-man-no',       action='store_true', help='without -VMKMAN=no')
    # parser.add_argument('--no-mk-share-no',     action='store_true', help='without -VMKSHARE=no')

    path_default = '${obj}/build_%Y%m%d-%H%M%S_${target}_${build_info}.log'
    parser.add_argument('--log', metavar='path', default=path_default, help='path to log file')

    parser.add_argument('target',     help='help_target')
    parser.add_argument('build_info', help='help_build_info')
    # @formatter:on

    parser.format_help()

    args, opts_other = parser.parse_known_args()

    args.O = args.O.replace('${mach}', args.m)
    args.D = args.D.replace('${mach}', args.m)
    args.R = args.R.replace('${mach}', args.m)

    args.log = args.log. \
        replace('${obj}', args.O). \
        replace('${target}', args.target). \
        replace('${build_info}', args.build_info)
    args.log = datetime.datetime.now().strftime(args.log)

    pairs_var_val = []
    if args.V is not None:
        for V in args.V:
            var, val = V.split('=')
            pairs_var_val.append((var, val))

    args.log = args.log.replace('<target>', args.target)
    args.log = args.log.replace('<build-info>', args.build_info)

    main(args.m, args.T, args.O, args.D, args.R, args.X,
         pairs_var_val,
         args.no_u, args.no_U, args.no_x,
         args.no_mk_crossgdb_yes,
         args.no_mk_debug_yes,
         # args.no_mk_doc_no,
         # args.no_mk_html_no,
         # args.no_mk_info_no,
         args.no_mk_kdebug_yes,
         # args.no_mk_lint_yes,
         # args.no_mk_man_no,
         # args.no_mk_share_no,
         args.log,
         opts_other,
         args.target, args.build_info)
