- [[podman]]
- linux容器
	- linux容器是一种虚拟化技术，LXC(Linux Containers)不是模拟一个完整的操作系统，而是对进程进行隔离，对于容器里的进程来说，各种资源是虚拟的，从而实现与底层系统的隔离。
	- 特点
		- 启动快
			- 容器里面的应用是底层系统的一个进程，因此容器可以达到进程级启动速度
		- 资源占用少
			- 相比于虚拟机，容器只占用需要的资源。另外，多个容器可以共享资源
		- 体积小
			- 相比于虚拟机，容器只包含用到的组件，因此相比虚拟机文件要小很多
- docker
	- docker 属于 Linux容器的一种封装，提供简单易用的容器使用接口，是目前最流行的linux容器解决方案
	- ![image.png](../assets/image_1673944206041_0.png)
	- ![image.png](../assets/image_1673944266512_0.png){:height 326, :width 593}
	- docker镜像
	  collapsed:: true
		- ![image.png](../assets/image_1673945888063_0.png){:height 304, :width 716}
		-
- docker的用途
	- 提供一次性环境
	- 提供弹性的云服务
	- 组件微服务架构
- 参考文档
	- [**Mini Container Series**](https://hechao.li/2020/06/09/Mini-Container-Series-Part-0-Not-a-Real-Container/)
	- [Container engines, runtimes and orchestrators: an overview](https://sarusso.github.io/blog/container-engines-runtimes-orchestrators.html)
	  id:: 65644675-7f26-472a-8bbc-3cd1f46e1d47
	- [每天5分钟玩转Docker容器技术](https://github.com/loveLynch/Book/blob/master/%E6%AF%8F%E5%A4%A95%E5%88%86%E9%92%9F%E7%8E%A9%E8%BD%ACDocker%E5%AE%B9%E5%99%A8%E6%8A%80%E6%9C%AF.CloudMan(%E8%AF%A6%E7%BB%86%E4%B9%A6%E7%AD%BE).pdf) #pdf #github
	- [每天5分钟玩转Kubernetes](https://github.com/YoungLC/ebooks/blob/master/docker%26K8S/%E6%AF%8F%E5%A4%A95%E5%88%86%E9%92%9F%E7%8E%A9%E8%BD%ACKubernetes.pdf) #pdf #github
	- [Docker 简易入门教程](https://iphysresearch.github.io/blog/post/programing/docker-tutorial/)
	- [Docker 入门教程](https://www.ruanyifeng.com/blog/2018/02/docker-tutorial.html)
	- [Docker 微服务教程](https://www.ruanyifeng.com/blog/2018/02/docker-wordpress-tutorial.html)
		- #+BEGIN_QUOTE
		  image 仓库的镜像网址
		  打开/etc/default/docker文件（需要sudo权限），在文件的底部加上一行
		  DOCKER_OPTS="--registry-mirror=https://registry.docker-cn.com"
		  重启 Docker 服务
		  sudo service docker restart
		  #+END_QUOTE
	- [docker Reference documentation](https://docs.docker.com/reference/)
	- [Digging Into Runtimes – runc](https://blog.quarkslab.com/digging-into-runtimes-runc.html)
	- [Digging into the OCI Image Specification](https://blog.quarkslab.com/digging-into-the-oci-image-specification.html)
	- [Digging into Linux namespaces - part 1](https://blog.quarkslab.com/digging-into-linux-namespaces-part-1.html)
	- [Digging into Linux namespaces - part 2](https://blog.quarkslab.com/digging-into-linux-namespaces-part-2.html)
	- [The differences between Docker, containerd, CRI-O and runc](https://www.tutorialworks.com/difference-docker-containerd-runc-crio-oci/)
	- [docker architecture](https://docs.docker.com/get-started/overview/#docker-architecture)
- [Learning Containers From The Bottom Up](https://iximiuz.com/en/posts/container-learning-path/)
- [A Practical Introduction to Container Terminology](https://developers.redhat.com/blog/2018/02/22/container-terminology-practical-introduction#)
- [Difference Between Docker Images and Containers](https://www.baeldung.com/ops/docker-images-vs-containers)
- OCI
  id:: 654cb239-caf4-4432-a388-d977ff7f17e1
	- [Open Container Initiative](https://github.com/opencontainers/image-spec/blob/main/spec.md#open-container-initiative)
	- [OCI Image Layout Specification](https://github.com/opencontainers/image-spec/blob/main/image-layout.md#oci-image-layout-specification)
	-
	- [Open Container Initiative Runtime Specification](https://github.com/opencontainers/runtime-spec/blob/main/spec.md)
	- [OCI runtime-spec config](https://github.com/opencontainers/runtime-spec/blob/main/config.md#configuration)
	- [COI runtime-spec Configuration Schema Example](https://github.com/opencontainers/runtime-spec/blob/main/config.md#configuration-schema-example)
	- [Runtime and Lifecycle](https://github.com/opencontainers/runtime-spec/blob/main/runtime.md#lifecycle)
	- [Modifying OCI Images the hard way](https://www.justinpolidori.it/posts/20230219_modifying_oci_images_the_hard_way/)
- [runc](https://github.com/opencontainers/runc)
	- #+BEGIN_QUOTE
	  runc is a CLI tool for spawning and running containers on Linux according to the OCI specification.
	  #+END_QUOTE
- [**Docker Deep Dive.pdf**](https://github.com/Ameen-Alam/CNC-Docker/blob/master/Docker%20Deep%20Dive.pdf)
- [**Docker Cookbook.pdf**](https://github.com/mr-nsin/references/blob/master/Docker%20Cookbook.pdf)
- [Most Popular Container Runtimes](https://www.cloudraft.io/blog/container-runtimes)
- [Overview of Different Container Runtimes](https://cto.ai/blog/overview-of-different-container-runtimes/)
- [Container Runtimes](https://kubernetes.io/docs/setup/production-environment/container-runtimes/)
- [Understanding containers](https://www.redhat.com/en/topics/containers?extIdCarryOver=true&sc_cid=701f2000001Css5AAC#overview)