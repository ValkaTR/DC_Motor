#!/usr/bin/gnuplot
# gnuplot file

set termoption dash
set decimalsign ","
set encoding utf8
set grid x y linewidth 0.5 linecolor rgb "#202020"

set title "Konstantne koormus"
#set title "Ventilaator koormus"

# for pdf
set key spacing 0.8 right center

set xrange [*:*]
set yrange [*:*]

set x2range [*:*]
set y2range [*:*]

#set logscale x

set xlabel "t, s"
#set ylabel "I, A"

#set x2label "t, ms"
#set y2label "I, A"

#set ytics nomirror
set xtics 0.2 rotate
#set mxtics 0
#set mytics 0
#set tics out

#set y2tics
#set x2tics

# for png
#set pointsize 5

# for pdf
set pointsize 1

#set ytics add ("14,9" 14.86)
#set xtics add ("0.074" 0.07773)

# for black and white
set style line 1 linetype 1 linecolor rgb "black"
set style line 2 linetype 1 linecolor rgb "red"
set style line 3 linetype 1 linecolor rgb "blue"
set style line 4 linetype 1 linecolor rgb "dark-green"
set style line 5 linetype 3 linecolor rgb "black"

#set terminal postscript eps enhanced font "Serif, 14" linewidth 1.5 color size 16.5cm, 16.5cm
#set output "template_osc.eps"

set terminal pngcairo enhanced notransparent font "Serif, 20" linewidth 2 size 1500, 1500
set output "DC_motor.png"

#set terminal pdfcairo enhanced color dashed font "CMU Serif, 12" linewidth 5 dashlength 1 size 17cm, 23.0cm
#set output "simulation_multi_osc.pdf"

#set terminal wxt enhanced size 1200,900 font "CMU Serif, 12"

## set the margin of each side of a plot as small as possible
## to connect each plot without space
w_top = 0.01
w_bottom = 0.06
w_a = 0.30
w_b = 0.35
w_c = 0.10
w_d = 0.25
#w_e = 2.0
#w_f = 2.2
w_sum = w_top + w_bottom + w_a + w_b + w_c + w_d #+ w_e + w_f

set lmargin 9
set rmargin 9

set multiplot

# Time stepback
stepback = 0

###############################################################################
# 1

set size 1.0, w_a / w_sum
set origin 0.0, 1.0 - ((w_top + w_a) / w_sum)

set tmargin w_top
set bmargin 0

set zeroaxis

#set xtics 0.5 rotate
unset xlabel
#set format x "%.2f"
set format x ""

##

unset y2tics
unset y2label
set ylabel "I, A"
set ytics  nomirror

set yrange [*:*]
unset grid
set grid x y

#set label '(a)' at -7.5, 12 font ",16"

plot \
'DC_motor.dat' using ($1-stepback):($3) title "Vool" with lines linestyle 1, \
84 title "I_{max}" with lines linestyle 2

###############################################################################
# 2
set size 1.0, w_b / w_sum
set origin 0.0, 1.0 - ((w_top + w_a + w_b) / w_sum)

set tmargin 0
set bmargin w_bottom

#set xtics rotate
#set xlabel "t, s"
#set format x "%.2f"

unset title
#unset label

##

unset y2tics
unset y2label
set ylabel "n, min¯¹"
set ytics 50 nomirror

set yrange [0:799]
unset grid
set grid x y

#set label '(d)' at -9.5, 350 font ",16"

plot \
'DC_motor.dat' using ($1-stepback):($4) title "Kiirus" with lines linestyle 1, \
0.5 * 710 title "0,5 · N_n" with lines linestyle 2, \
1.0 * 710 title "1,0 · N_n" with lines linestyle 4

###############################################################################
# 3
set size 1.0, w_c / w_sum
set origin 0.0, 1.0 - ((w_top + w_a + w_b + w_c) / w_sum)

set tmargin 0
set bmargin w_bottom

#set xtics rotate
#set xlabel "t, s"
#set format x "%.2f"

#unset label

##

unset y2tics
unset y2label
set ylabel "–"
set ytics 1 nomirror

set yrange [-0.5:1.5]
unset grid
set grid x y

#set label '(d)' at -9.5, 350 font ",16"

plot 'DC_motor.dat' using ($1-stepback):($6) title "Voolupiirang" with lines linestyle 1

###############################################################################
# 4
set size 1.0, w_d / w_sum
set origin 0.0, 1.0 - ((w_top + w_a + w_b + w_c + w_d) / w_sum)

set tmargin 0
set bmargin w_bottom

#set xtics rotate
set xlabel "t, s"
set format x "%.2f"

#unset label

##

unset y2tics
unset y2label
set ylabel "PWM, –"
set ytics 0.1 nomirror

set yrange [0:1.05]
unset grid
set grid x y

plot \
'DC_motor.dat' using ($1-stepback):($5) title "PWM" with lines linestyle 1

###############################################

#unset multiplot

#pause -1 "Press any key to continue..."

################################################
