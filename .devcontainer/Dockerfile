FROM px4io/px4-dev-nuttx-focal:2022-08-12

# Keep the original UID/GID by renaming the pre-existing 'user' account to 'wrj'.
# This avoids UID conflicts while matching the expected username/home path.
RUN set -eux; \
    if id -u user >/dev/null 2>&1; then \
        usermod -l wrj user; \
        if getent group user >/dev/null 2>&1; then groupmod -n wrj user; fi; \
        usermod -d /home/wrj -m wrj; \
    elif ! id -u wrj >/dev/null 2>&1; then \
        useradd -m -s /bin/bash wrj; \
    fi
