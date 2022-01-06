#!/usr/bin/env sh
set -euo pipefail

PACKAGE_LICENSE="(MIT WITH selva-exception) OR AGPL-3.0-only"
DEFAULT_LICENSE="(MIT WITH selva-exception) OR AGPL-3.0-only" # Default license for source files
OUTPUT_FILE="module.spdx" # Default SPDX Document output file

declare -A path_licenses=()

# Parse SPDX-License-Identifier from a file.
# @param 1 is the filepath
function parse_file_license {
    local license=$(grep "SPDX-License-Identifier:" "$1" | sed 's/.*SPDX-License-Identifier: \([^*]*\).*$/\1/')
    echo "$license"
}

# Calculate file SHA1.
# @param 1 is the filepath
function file_sha1 {
    local sha=($(sha1sum -b "$1"))
    echo "$sha"
}

# Sometimes there is a license file for the files under a path.
function find_path_licenses {
    trap "$(shopt -p lastpipe)" RETURN
    shopt -s lastpipe

    find . -type f \( -name "COPYING" -o -name "LICENSE" \) -print0 | sort | while read -d $'\0' file; do
        local license="$(parse_file_license "$file")"

        path_licenses[$file]="$license"
    done
}

function upsearch_license {
    local startdir="$1"
    local stopdir="$2"

    local directory="$startdir"
    while directory=$(realpath --relative-to="$stopdir" "$directory/.." 2> /dev/null); do
        if [[ "$directory" == "." || "$directory" == '..' ]]; then return 1; fi
        test -e "$directory/COPYING" && echo "$directory/COPYING" && return
        test -e "$directory/LICENSE" && echo "$directory/LICENSE" && return
    done

    return 1
}

# Try to guess a file license by the filepath
# @param 1 is the filepath
function guess_missing_file_license {
    local filepath="${1%/*}"
    local lic_path="$(upsearch_license "$filepath" '.')"

    if [ ! -z "$lic_path" ]; then
        lic_path="./$lic_path" # it actually has ./ in front as we don't strip it
        [ ${path_licenses[$lic_path]+abc} ] && echo "${path_licenses[$lic_path]}"
    fi
    return 1
}

function check_missing_tags {
    pushd "$1" &>/dev/null
    local x=$(grep -rL --include="*$2" "SPDX-License-Identifier:" .)
    echo "$(echo "$x" | wc -l)"
    if [ "$(echo "$x" | wc -l)" -gt 1 ]; then
        echo 'Some files are missing the SPDX license identifier comment:'
        echo "$x"
    fi
    popd &>/dev/null
}

# Generate the SPDX Document info.
function gen_spdx_info {
    {
        echo 'SPDXVersion: SPDX-2.2'
        echo 'DataLicense: CC0-1.0'
        echo 'SPDXID: SPDXRef-DOCUMENT'
        echo 'DocumentName: selva-module'
        echo 'DocumentNamespace: https://spdx.org/spdxdocs' # TODO
        echo 'Creator: Organization: SAULX'
        echo "Created: $(date -u +"%Y-%m-%dT%H:%M:%SZ")"
        echo ''
    } >&3
}

# Generate the SPDX package info for selva-module.
function gen_sdpx_package {
    {
        echo '##### Package: selva-module'
        echo ''
        echo 'PackageName: selva-module'
        echo 'SPDXID: SPDXRef-Package-selva-module'
        echo 'PackageDownloadLocation: NONE'
        echo 'FilesAnalyzed: true'
        # TODO echo "PackageVerificationCode: "
        echo "PackageLicenseConcluded: $PACKAGE_LICENSE"
        echo 'PackageLicenseInfoFromFiles: NOASSERTION'
        echo "PackageLicenseDeclared: $PACKAGE_LICENSE"
        echo "PackageCopyrightText: NOASSERTION" # TODO
        echo ''
        echo 'Relationship: SPDXRef-DOCUMENT DESCRIBES SPDXRef-Package-selva-module'
        echo ''
    } >&3
}

# Generate SPDX file info for each source file.
# @param 1 soure filename suffix
function gen_spdx_sources {
    pushd "$1" &>/dev/null
    find . -name "*$2" -print0 | sort | while read -d $'\0' file
    do
        local fullpath="$(realpath --relative-to=$1 "$file")"
        local license="$(parse_file_license $file)"
        local copyright=$(grep "Copyright (" "$file" | sed 's/^[^C]*//')
        local sha="$(file_sha1 "$file")"

        if [ -z "$license" ]; then
            local license_concluded="$(guess_missing_file_license "$file" || echo "$DEFAULT_LICENSE")"
            local license_in_file="NONE"
        else
            local license_concluded="$license"
            local license_in_file="$license"
        fi
        if [ -z "$copyright" ]; then
            copyright="NONE"
        else
            copyright="<text>$copyright</text>"
        fi

        {
            echo "FileName: $fullpath"
            echo "SPDXID: SPDXRef-$sha-src"
            echo 'FileType: SOURCE'
            echo "FileChecksum: SHA1: $sha"
            echo "LicenseConcluded: $license_concluded"
            echo "LicenseInfoInFile: $license_in_file"
            echo "FileCopyrightText: $copyright"
            echo ''
        } >&3
    done
    popd &>/dev/null
}

# Add a BINARY file info section.
# @param 1 filepath
# @param 2 license
function add_spdx_lib {
    local filepath="$1"
    local filename=$(basename -- "$filepath")
    filename="${filename%.*}"
    local sha="$(file_sha1 "$filepath")"
    local license="$2"

    {
        echo "FileName: $filepath"
        echo "SPDXID: SPDXRef-$filename-binary"
        echo 'FileType: BINARY'
        echo "FileChecksum: SHA1: $sha"
        echo "LicenseConcluded: $license"
        echo 'LicenseInfoInFile: NOASSERTION'
        echo 'FileCopyrightText: NOASSERTION'
        echo ''
    } >&3
}

# Add main binary file info section
# @param 1 filepath
# @param 2 license
function add_spdx_main {
    local filepath="$1"
    local filename=$(basename -- "$filepath")
    filename="${filename%.*}"
    local sha="$(file_sha1 "$filepath")"
    local license="$2"

    {
        echo "FileName: $filepath"
        echo 'SPDXID: SPDXRef-main-binary'
        echo 'FileType: BINARY'
        echo "FileChecksum: SHA1: $sha"
        echo "LicenseConcluded: $license"
        echo 'LicenseInfoInFile: NOASSERTION'
        echo 'FileCopyrightText: NOASSERTION'
        echo ''
    } >&3
}

#check_missing_tags "$1" ".c"

rm -f "$OUTPUT_FILE"
exec 3> "$OUTPUT_FILE"

find_path_licenses
gen_spdx_info
# TODO Add exception text
gen_sdpx_package
# TODO Add Makefiles
add_spdx_lib ../binaries/linux_x64/libhiredis.so "BSD-3-Clause"
add_spdx_main ../binaries/linux_x64/redis-server-selva "BSD-3-Clause"
gen_spdx_sources "." ".h"
gen_spdx_sources "." ".c"
# TODO Add relationships for Makefiles

exec 3>&-
