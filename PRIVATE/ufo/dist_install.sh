#! /bin/bash

# dist_install.sh - Copy binary libraries from repository to their output
# locations (typically under ${ANDROID_PRODUCT_OUT}).  Uncompress as needed.
# If an identical file already exists in output location, do not disturb it.
#
# Args:
# 1. Directory containing input files in locations relative to their eventual
#    locations under ${ANDROID_PRODUCT_OUT}.  If blank, {script_directory}/dist
# 2. Directory under which files will be installed.
#    If blank, defaults to ${ANDROID_PRODUCT_OUT}

set -e
set -u

proc_path="${BASH_SOURCE[0]}"
pnm="${proc_path##*/}"

opt_help=0
opt_dry_run=0
opt_diff=0
opt_rm=0
dir_from=""
dir_to=""
opt_verbose=1

#strip symbol for user build
opt_strip_sym=0
#when do file command, binaries will have ELF in the output
#use this as regex for determining binaries for strip
regex_bin="ELF"

param_list=( "${@:+${@}}" )

errdet=0
nparams=0
for param in "${param_list[@]:+${param_list[@]}}" ; do
    kw_arg="${param#*=}"
    if [ "${param}" == "${kw_arg}" ] ; then
	kw_arg=""
    fi
    case "${param}" in
        -h|--help)
            opt_help=1
            ;;
        --diff)
            opt_diff=1
            ;;
        --no-diff)
            opt_diff=0
            ;;
        --rm)
            opt_rm=1
            ;;
        --no-rm)
            opt_rm=0
            ;;
        --dry-run)
            opt_dry_run=1
            ;;
        --no-dry-run)
            opt_dry_run=0
            ;;
        --strip-symbols)
            opt_strip_sym=1
            ;;
        -q)
            opt_verbose=$((opt_verbose-1))
            ;;
        -v)
            opt_verbose=$((opt_verbose+1))
            ;;
        -*)
            echo >&2 "${pnm}: Error: Unrecognized option: ${param}"
            errdet=1
            ;;
        *)
            case "${nparams}" in
                0)
                    dir_from="${param}"
                    ;;
                1)
                    dir_to="${param}"
                    ;;
                *)
                    echo >&2 "${pnm}: Error: Unexpected parameter: ${param}"
                    errdet=1
                    ;;
            esac
            nparams=$((nparams+1))
            ;;
    esac
done

if [ ${opt_help} -ne 0 ] ; then
    cat <<EOF
Install ufo distribution files under \${ANDROID_PRODUCT_OUT}.

usage: $0 [options] [dir_from] [dir_to]

dir_from is the directory containing the ufo library binary distribution,
which is usually ./dist, relative to the ufo binary repostory.

dir_to is usually \${ANDROID_PRODUCT_OUT}.

Options are:
-h                Output this help text.
--help            Output this help text.
--diff            Compare files.  Do not transfer them.
--no-diff         Transfer the files.
--rm              Remove target files.
--no-rm           Do not remove target files.
--dry-run         Avoid copying or changing any files
--no-dry-run
--strip-symbols   strip all symbols from binaries
-q                Be less verbose
-v                Be more verbose.
EOF
    exit
fi

if [ ${errdet} -ne 0 ] ; then
    echo >&2 "${pnm}: Aborting due to previously detected error(s)"
    exit 1
fi

if [ -z "${dir_from}" ] ; then
    dir_from=$( dirname "${proc_path}" )
    if [ "${dir_from}" == "." ] ; then
        dir_from="dist"
    else
        dir_from="${dir_from}/dist"
    fi
fi

if [ -z "${dir_to}" ] ; then
    dir_to=${ANDROID_PRODUCT_OUT}
fi

if [ ! -d "${dir_from}" ] ; then
    echo >&2 "${pnm}: Error: Required directory not present: \"${dir_from}\""
    exit 1
fi

# Does not duplicate symbolic links yet.

dist_list=( $( ( cd "${dir_from}" ; find . -type f -print | sed -e 's%^./%%' ) ) )

for dist_file in "${dist_list[@]:+${dist_list[@]}}" ; do
    if [ "${dir_from}" == "." ] ; then
        path_from="${dist_file}"
    else
        path_from="${dir_from}/${dist_file}"
    fi

    file_ext="${path_from##*.}"
    if [ "${file_ext}" == "${path_from}" ] ; then
        file_ext=""
    fi

    if [ ! -r "${path_from}" ] ; then
        echo >&2 "${pnm}: Error: Input file not found: ${path_from}"
        continue
    fi

    flag_redirect=1
    case "${file_ext}" in
        bz2)
            cpcmd="bzip2 -dc"
            pgm_diff=xzdiff
            ;;
        gz)
            cpcmd="gzip -dc"
            # In case system does not have xzdiff installed.
            pgm_diff=zdiff
            ;;
        xz)
            cpcmd="xz -dc"
            pgm_diff=xzdiff
            ;;
        *)
            cpcmd="cp"
            pgm_diff=diff
            flag_redirect=0
            file_ext=""
            ;;
    esac

    path_to="${dir_to}/${dist_file}"
    if [ -n "${file_ext}" ] ; then
        path_to="${path_to%.*}"
    fi

    # If the file is already there, do not update it unless it is different.
    # This will help to avoid needless rebuilds by users of the libraries.

    # xzdiff recognizes compression types none, xz, lzma, gzip, bzip2, and lzop.
    if [ ! -e "${path_to}" ] ; then
        files_are_identical=missing_to
    elif ${pgm_diff} -q "${path_from}" "${path_to}" >/dev/null ; then
        # Files are the same.  Take no action.
        files_are_identical=y
    else
        files_are_identical=n
    fi

    if [ ${opt_diff} -ne 0 ] ; then
        if [ "${files_are_identical}" == "y" ] ; then
            if [ ${opt_verbose} -ge 1 ] ; then
                echo >&2 "${pnm}: Info: File  same:   ${path_from} ${path_to}"
            fi
        elif [ "${files_are_identical}" == "n" ] ; then
            echo >&2 "${pnm}: Info: Files differ: ${path_from} ${path_to}"
        elif [ "${files_are_identical}" == "missing_to" ] ; then
            echo >&2 "${pnm}: Info: File not present:  ${path_from}"
        else
            echo >&2 "${pnm}: Info: unknown diff issue: ${files_are_identical} : ${path_from} ${path_to}"
        fi
    elif [ ${opt_rm} -ne 0 ] ; then
        if [ ! -e "${path_to}" ] ; then
            echo >&2 "${pnm}: Info: File already absent: ${path_to}"
        else
            echo >&2 "${pnm}: Info: Removing: ${path_to}"
            if [ ${opt_verbose} -ge 1 ] || [ ${opt_dry_run} -ne 0 ] ; then
                echo >&2 "+ " rm "${path_to}"
            fi
            if [ ${opt_dry_run} -eq 0 ] ; then
                rm "${path_to}"
            fi
        fi
    else
        if [ "${files_are_identical}" == "y" ] ; then
            if [ ${opt_verbose} -ge 1 ] ; then
                echo >&2 "${pnm}: Info: File is unchanged: ${path_from}"
            fi
        else
            if [ "${files_are_identical}" != "n" ] && [ "${files_are_identical}" != "missing_to" ] ; then
                echo >&2 "${pnm}: Info: unknown diff problem: ${files_are_identical} : ${path_from} ${path_to}"
            fi

            path_to_dir=$( dirname "${dir_to}/${dist_file}" )
            if [ ! -d "${path_to_dir}" ] ; then
                if [ ${opt_verbose} -ge 1 ] || [ ${opt_dry_run} -ne 0 ] ; then
                    echo >&2 "+ " mkdir -p "${path_to_dir}"
                fi
                if [ ${opt_dry_run} -eq 0 ] ; then
                    mkdir -p "${path_to_dir}"
                fi
            fi


            if [ ${flag_redirect} -eq 0 ] ; then
                if [ ${opt_verbose} -ge 1 ] || [ ${opt_dry_run} -ne 0 ] ; then
                    echo >&2 "+ " ${cpcmd} "${path_from}" "${path_to}"
                    #strip binary symbols if requested, ignore binary test for dry-run
                    if [ ${opt_strip_sym} -eq 1 ] ; then
                        echo >&2 "+ " strip -s "${path_to}"
                    fi

                fi
                if [ ${opt_dry_run} -eq 0 ] ; then
                    ${cpcmd} "${path_from}" "${path_to}"
                    bin_test=$( file ${path_to} )
                    #strip symbols if requested&&file is a binary
                    if [ ${opt_strip_sym} -eq 1 ] && [[ "${bin_test}" =~ ${regex_bin} ]] ; then
                        strip -s "${path_to}"
                    fi
                fi
            else
                if [ ${opt_verbose} -ge 1 ] || [ ${opt_dry_run} -ne 0 ] ; then
                    echo >&2 "+ " ${cpcmd} "${path_from}" ">${path_to}"
                    #strip binary symbols if requested, ignore binary test for dry-run
                    if [ ${opt_strip_sym} -eq 1 ] ; then
                        echo >&2 "+ " strip -s "${path_to}"
                    fi
                fi
                if [ ${opt_dry_run} -eq 0 ] ; then
                    ${cpcmd} "${path_from}" >"${path_to}"
                    #strip symbols if requested&&file is a binary
                    bin_test=$( file ${path_to} )
                    if [ ${opt_strip_sym} -eq 1 ] && [[ "${bin_test}" =~ ${regex_bin} ]] ; then
                        strip -s "${path_to}"
                    fi
                fi
            fi
        fi
    fi
done
