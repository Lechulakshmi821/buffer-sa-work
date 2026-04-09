set terminal pngcairo size 1000,600
set output 'throughput_line_graph.png'
set title 'CPU vs GPU Decompression Throughput'
set xlabel 'Distribution'
set ylabel 'Throughput (MB/s)'
set grid ytics
set key left top
set xtics ('Repeated' 0, 'Uniform' 1, 'Sorted' 2, 'Skewed' 3)
plot 'plot_data.dat' using 1:3 with linespoints lw 2 pt 7 title 'CPU', \
     'plot_data.dat' using 1:4 with linespoints lw 2 pt 7 title 'GPU'
