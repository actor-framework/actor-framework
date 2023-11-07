FROM mcr.microsoft.com/dotnet/framework/sdk:4.8-windowsservercore-ltsc2019

ADD ./robot/dependencies.txt .

SHELL [ "powershell" ]

RUN Set-ExecutionPolicy Unrestricted -Force

# Get Chocolatey to install other dependencies.
RUN Set-ExecutionPolicy Bypass -Scope Process -Force; \
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; \
    iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

# Install the Visual Studio tooling
RUN choco install -y --no-progress visualstudio2022buildtools
RUN choco install -y --no-progress visualstudio2022-workload-vctools

# Install Git, OpenSSL and Python.
RUN choco install -y --no-progress msysgit openssl python

# Install Robot Framework and dependencies.
RUN python.exe -m pip install -r dependencies.txt

# Add git to the global PATH
SHELL [ "cmd", "/c" ]
RUN setx /M PATH "%PATH%;C:\\Program Files\\Git\\bin"
