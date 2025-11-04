all:
	g++ -std=c++17 rto.cpp -O3 -o main
	./main input.txt
clean:
	rm output.ppm
	rm main