# Xcast6 Treemap overview
Xcast6 Treemap is a new multicast protocol designed for IPv6. More information about Xcast can be found in:
- (slides at Coseners 2016) http://coseners.net/wp-content/uploads/2015/07/KhoaPhan_Xcast_UCL.pdf
- (poster at CoNEXT 2012) http://hal.inria.fr/docs/00/74/92/66/PDF/stud16-phan.pdf
- (paper at CCNC 2010) ftp://ftp-sop.inria.fr/members/Truong_Khoa.Phan/CCNC10_Phan_Xcast6Treemap.pdf

# Xcast6 Treemap source code and installation
- Xcast endhost code was tested with linux kernel 2.6.21 and Xcast router code was tested with FreeBSD version 6.2. 
- Installation:
  - Xcast endhost:
    - download and extract kernel linux2.6.21
    - patch kernel using command: patch -p1 < linux-2.6.21-xc6-treemap.patch
    - rebuild kernel using following commands:
      - make menuconfig: 
	      - Networking
		      - Networking Options:
			      - set "y" for IPv6 and XCAST6 TREEMAP
      - make
      - make modules
      - make modules_install
      - make install
  - LibXcast: make & make install.
    - Test for sending: gcc -o test test.c -lxcast
    - Test for receving: gcc -o recv xrecv.c -lxcast
  - Xcast router: patch the diff file and recompile kernel
    
