#!/bin/bash

export http_proxy="http://proxy-chain.intel.com:911"
export https_proxy="http://proxy-chain.intel.com:911"
export ftp_proxy="http://proxy-chain.intel.com:911"
export no_proxy="localhost,127.0.0.1/8,.ka.intel.com,.ir.intel.com,devtools.intel.com"

echo wlc123 | sudo -S apt-get update -y
sudo apt install mosquitto mosquitto-clients -y
#sudo systemctl status mosquitto
sudo apt install openssl libssl-dev -y
sudo apt-get install libglew-dev -y
sudo apt-get install libsdl2-dev -y
sudo apt-get install libglm-dev -y
sudo apt install git-all -y

# git configuration
#sudo mkdir ~/bin
#sudo cp /mnt/git-proxy ~/bin
#sudo git config --global core.gitProxy ${HOME}/bin/git-proxy
#sudo chmod +x ${HOME}/bin/git-proxy


