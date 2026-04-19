set terminal pngcairo size 1000,600
set output 'ratio_bar_plot.png'
set title 'RLE Throughput vs CPU:GPU Ratio (Average of 10 Runs)'
set xlabel 'CPU:GPU Ratio'
set ylabel 'Throughput (MB/s)'
set grid ytics
set boxwidth 0.6
set style fill solid border -1
set yrange [0:*]
set xtics rotate by -45
set xtics ('100:0' 0, '75:25' 1, '50:50' 2, '25:75' 3, '0:100' 4)
plot 'ratio_plot_data.dat' using 1:2 with boxes title 'Average Throughput', \
     '' using 1:2:3:4 with yerrorbars notitle
