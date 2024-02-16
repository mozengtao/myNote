- What is Podman?
	- #+BEGIN_QUOTE
	  Podman (the POD MANager) is **a tool for managing containers and images, volumes mounted into those containers, and pods made from groups of containers**. Podman runs containers on Linux, but can also be used on Mac and Windows systems using a Podman-managed virtual machine. Podman is based on libpod, a library for container lifecycle management that is also contained in this repository. The libpod library provides APIs for managing containers, pods, container images, and volumes.
	  
	  Podman (short for Pod Manager) is a container engine
	  
	  #+END_QUOTE
- podman-load
	- ```bash
	  podman-load - Load image(s) from a tar archive into container storage
	  
	  SYNOPSIS
	         podman load [options]
	         podman image load [options]
	  
	     --input, -i=input
	         Read from archive file, default is STDIN.
	  ```
- podman-run
	- ```bash
	  # podman-run - Run a command in a new container
	  
	  # SYNOPSIS
	         podman run [options] image [command [arg ...]]
	         podman container run [options] image [command [arg ...]]
	  
	     --name=name
	         Assign a name to the container.
	  
	     --privileged=true|false
	         Give extended privileges to this container. The default is false.
	  
	     --memory, -m=number[unit]
	         Memory limit. A unit can be b (bytes), k (kilobytes), m (megabytes), or g (gigabytes).
	  
	     --env, -e=env
	         Set environment variables.
	  
	     --volume, -v[=[[SOURCE-VOLUME|HOST-DIR:]CONTAINER-DIR[:OPTIONS]]]
	         Create a bind mount. If you specify /HOST-DIR:/CONTAINER-DIR, Podman bind mounts 
	         host-dir in the host to CONTAINER-DIR in the Podman container. Similarly, 
	         SOURCE-VOLUME:/CONTAINER-DIR will mount  the  volume  in the host to the container. 
	         If no such named volume exists, Podman will create one. (Note when using the remote 
	         client, the volumes will be mounted from the remote server, not necessarily the 
	         client machine.)
	  
	     --detach, -d=true|false
	         Detached mode: run the container in the background and print the new container ID. 
	         The default is false.
	  ```
	- ```bash
	  # Example
	  podman run
	  --name $vm_name
	  --privileged
	  --memory 2G
	  
	  -e IS_PRINCIPAL=1
	  -e NOMAD_CPU_CORES=20-23
	  -e NET_PCI=0000:0b:11.6
	  -e RPHY_PCI=0000:0b:01.6
	  -e RPHY_IP=5.5.5.5
	  -e NSI_IP=1.1.1.1
	  -e RPD_IP=6.6.6.6 
	  -e DHCP_SERVER_MAC=d0:99:d5:bb:ca:4a
	  -e DHCP_SERVER_IP=172.25.2.7 
	  
	  -v /home/vlv/conf/cms/loglvl.json:/usr/share/cms/loglvl.json 
	  -v /home/vlv/conf/docsis-mac/loglvl.json:/usr/share/docsis-mac/loglvl.json 
	  -v /home/vlv/conf/vcmts/vcmts.cfg:/usr/share/vcmts/vcmts.cfg 
	  -v /home/vlv/conf/vcmts/vcmts.json:/usr/share/vcmts/vcmts.json 
	  -v /dev/hugepages:/mnt/huge 
	  -v /home/vlv/images/cms:/usr/bin/cms
	  
	  -d localhost/$vm_image
	  ```
- podman-top
	- ```bash
	  podman-top - Display the running processes of a container
	  
	  SYNOPSIS
	         podman top [options] container [format-descriptors]
	         podman container top [options] container [format-descriptors]
	  
	  descriptors that are supported
	  hpid
	         The corresponding host PID of a container process.
	  comm
	  	   corresponding command of the process
	  ```
- [podman command](https://docs.podman.io/en/latest/markdown/podman.1.html)
- [Podman Tutorial](https://www.ionos.co.uk/digitalguide/server/tools/podman-tutorial/)
- [podman github](https://github.com/containers/podman)
- [podman docs](https://docs.podman.io/en/latest/)
- [Getting Started with Podman](https://podman.io/docs#getting-help)
- [podman-run](https://docs.podman.io/en/latest/markdown/podman-run.1.html)
- [conmon](https://github.com/containers/conmon#conmon)
	> An OCI container runtime monitor.
	  Conmon is a monitoring program and communication tool between a container manager (like Podman or CRI-O) and an OCI runtime (like runc or crun) for a single container.
- ((654cb239-caf4-4432-a388-d977ff7f17e1))
- [Basic Networking Guide for Podman](https://github.com/containers/podman/blob/main/docs/tutorials/basic_networking.md)
- [Understanding Bind Mounts](https://www.baeldung.com/linux/bind-mounts)