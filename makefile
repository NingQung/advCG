all:
	g++ -std=c++17 rto.cpp -O3 -o main
c1:
	./main hw2_input_100.txt
c2:
	./main hw2_input_999G3.txt
c3:
	./main hw2_input_1000.txt
	
clean:
	rm output.ppm
	rm main