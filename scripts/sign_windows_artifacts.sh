#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: WINDOWS_CODE_SIGNING_CERTIFICATE_PASSWORD=... $0 <certificate.pfx> <file-or-directory>..." >&2
    exit 2
fi

certificate_path="$1"
shift

certificate_password="${WINDOWS_CODE_SIGNING_CERTIFICATE_PASSWORD:-}"
timestamp_url="${WINDOWS_CODE_SIGNING_TIMESTAMP_URL:-http://timestamp.digicert.com}"
description="${WINDOWS_CODE_SIGNING_DESCRIPTION:-ORNLSlicer}"
publisher_url="${WINDOWS_CODE_SIGNING_PUBLISHER_URL:-https://github.com/ORNLSlicer/ORNLSlicer}"

if [[ ! -s "${certificate_path}" ]]; then
    echo "Code signing certificate '${certificate_path}' does not exist or is empty." >&2
    exit 2
fi

if [[ -z "${certificate_password}" ]]; then
    echo "Code signing certificate password is empty." >&2
    exit 2
fi

if ! command -v osslsigncode >/dev/null 2>&1; then
    echo "osslsigncode is required to sign Windows artifacts." >&2
    exit 2
fi

password_file="$(mktemp)"
trap 'rm -f "${password_file}"' EXIT
chmod 600 "${password_file}"
printf '%s' "${certificate_password}" > "${password_file}"

declare -a artifacts=()

for path in "$@"; do
    if [[ -d "${path}" ]]; then
        while IFS= read -r -d '' artifact; do
            artifacts+=("${artifact}")
        done < <(find "${path}" -type f -iname '*.exe' -print0 | sort -z)
    elif [[ -f "${path}" ]]; then
        artifacts+=("${path}")
    else
        echo "Artifact '${path}' does not exist." >&2
        exit 2
    fi
done

if [[ ${#artifacts[@]} -eq 0 ]]; then
    echo "No Windows executables found to sign." >&2
    exit 2
fi

for artifact in "${artifacts[@]}"; do
    tmp="$(mktemp --tmpdir="$(dirname "${artifact}")" "$(basename "${artifact}").signed.XXXXXX")"
    rm -f "${tmp}"

    echo "Signing ${artifact}"
    if ! osslsigncode sign \
        -pkcs12 "${certificate_path}" \
        -readpass "${password_file}" \
        -n "${description}" \
        -i "${publisher_url}" \
        -h sha256 \
        -ts "${timestamp_url}" \
        -in "${artifact}" \
        -out "${tmp}"; then
        rm -f "${tmp}"
        exit 1
    fi

    mv "${tmp}" "${artifact}"
    osslsigncode verify -in "${artifact}"
done
