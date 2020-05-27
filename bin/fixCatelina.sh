# #!/bin/bash

# echo 'update brew'

# brew update

# echo 'upgrade brew'

# brew upgrade

# echo 'brew install openssl'

# brew reinstall openssl@1.1


echo 'backup existing lib files, if they exist'

if [ -f /usr/local/lib/libssl.dylib ]; then
    mv /usr/local/lib/libssl.dylib /usr/local/lib/libssl_bak.dylib
fi

if [ -f /usr/local/lib/libcrypto.dylib ]; then
    mv /usr/local/lib/libcrypto.dylib /usr/local/lib/libcrypto_bak.dylib
fi

# Leaving some commands to check directory contents 
# ls -al /usr/local/Cellar/openssl@1.1/1.1.1d/lib
# ls -al /usr/local/lib/libssl* && ls -al /usr/local/lib/libcrypto*

echo 'add symlink to missing openssl libs'

if [ -f /usr/local/Cellar/openssl@1.1/1.1.1g/lib/libssl.1.1.dylib ]; then
    sudo ln -s /usr/local/Cellar/openssl@1.1/1.1.1g/lib/libssl.1.1.dylib /usr/local/lib/libssl.dylib
fi 

if [ -f /usr/local/Cellar/openssl@1.1/1.1.1g/lib/libcrypto.1.1.dylib ]; then
    sudo ln -s /usr/local/Cellar/openssl@1.1/1.1.1g/lib/libcrypto.1.1.dylib /usr/local/lib/libcrypto.dylib
fi