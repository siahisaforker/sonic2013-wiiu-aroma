# PowerShell helper to build the Docker image and run the container to build the Wii U binary.
docker build -t sonic-wiiu -f Dockerfile.wiiu .

# Mount the current working directory into the container. On Windows this uses the Windows path.
$hostPath = ${PWD}.Path
docker run --rm -v "$hostPath:/app" sonic-wiiu
