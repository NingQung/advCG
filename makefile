rtw2:
	g++ -std=c++17 rto.cpp -O3 -o main
	./main > image.ppm
rtw3: 
	g++ -std=c++17 main.cc -O3 -o main
	./main > image2.ppm