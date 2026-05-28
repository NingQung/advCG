rtw2:
	g++ -std=c++17 rto.cpp -O3 -o main
	./main > image.ppm
rtw3: 
	g++ -std=c++17 main.cc -O3 -o main
	./main > image2.ppm
spr:
	g++ -std=c++17 main.cc external/rgb2spec.cpp -O3 -pthread -o main
out1:
	./main > image3.ppm
out2:
	./main test1.obj 100 278 100 278 > image3.ppm