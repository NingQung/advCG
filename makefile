all:
	g++ -std=c++17 rto.cpp -O3 -o main
	./main > image.ppm