set terminal pngcairo size 1000,600
set output 'ratio_bar_plot.png'
set title 'RLE Throughput vs CPU:GPU Ratio'
set xlabel 'CPU:GPU Ratio'
set ylabel 'Throughput (MB/s)'
set style data histograms
set style fill solid border -1
set boxwidth 0.7
set grid ytics
plot 'ratio_plot_data.dat' using 2:xtic(1) title 'Throughput'
