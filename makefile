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
	./main assets/test2.obj 150 278 150 278 > image3.ppm
out2-a:
	./main assets/test4.obj 150 278 150 278 > image3.ppm
out3:
	./main assets/test3.obj 150 150 0 0 > image3.ppm
out-con:
	./main assets/test3.obj 150 150 0 0 > image3.ppm
	./main2 assets/test3.obj 150 150 0 0 > image4.ppm
	./main3 assets/test3.obj 150 150 0 0 > image5.ppm