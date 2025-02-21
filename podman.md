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
		# commands
		attach       (Attach to a running container)                                    
		auto-update  (Auto update containers according to their auto-update policy)     
		build        (Build an image using instructions from Containerfiles)            
		commit       (Create new image based on the changed container)                  
		container    (Manage containers)                                                
		cp           (Copy files/folders between a container and the local filesystem)  
		create       (Create but do not start a container)                              
		diff         (Display the changes to the object's file system)                  
		events       (Show podman events)                                               
		exec         (Run a process in a running container)                             
		export       (Export container's filesystem contents as a tar archive)          
		generate     (Generate structured data based on containers, pods or volumes)    
		healthcheck  (Manage health checks on containers)                               
		help         (Help about any command)                                           
		history      (Show history of a specified image)                                
		image        (Manage images)                                                    
		images       (List images in local storage)                                     
		import       (Import a tarball to create a filesystem image)                    
		info         (Display podman system information)                                
		init         (Initialize one or more containers)                                
		inspect      (Display the configuration of object denoted by ID)                
		kill         (Kill one or more running containers with a specific signal)       
		load         (Load image(s) from a tar archive)                                 
		login        (Login to a container registry)                                    
		logout       (Logout of a container registry)                                   
		logs         (Fetch the logs of one or more containers)                         
		machine      (Manage a virtual machine)                                         
		manifest     (Manipulate manifest lists and image indexes)                      
		mount        (Mount a working container's root filesystem)                      
		network      (Manage networks)
		pause        (Pause all the processes in one or more containers)
		play         (Play containers, pods or volumes from a structured file)
		pod          (Manage pods)
		port         (List port mappings or a specific mapping for the container)
		ps           (List containers)
		pull         (Pull an image from a registry)
		push         (Push an image to a specified destination)
		rename       (Rename an existing container)
		restart      (Restart one or more containers)
		rmi          (Removes one or more images from local storage)
		rm           (Remove one or more containers)
		run          (Run a command in a new container)
		save         (Save image(s) to an archive)
		search       (Search registry for image)
		secret       (Manage secrets)
		start        (Start one or more containers)
		stats        (Display a live stream of container resource usage statistics)
		stop         (Stop one or more containers)
		system       (Manage podman)
		tag          (Add an additional name to a local image)
		top          (Display the running processes of a container)
		unmount      (Unmounts working container's root filesystem)
		unpause      (Unpause the processes in one or more containers)
		unshare      (Run a command in a modified user namespace)
		untag        (Remove a name from a local image)
		version      (Display the Podman version information)
		volume       (Manage volumes)
		wait         (Block on one or more containers)

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