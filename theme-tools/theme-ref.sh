#!/bin/bash
#yellow text is brown
#bolded yellow text is yellow
#bolded green text is gray
#black background is transparent

CTR=1

for j in {0..7}
do

	if [ $j -eq 0 ]
	then
		fg="Black"
	fi

	if [ $j -eq 1 ]
	then
		fg="Red"
	fi

	if [ $j -eq 2 ]
	then
		fg="Green"
	fi

	if [ $j -eq 3 ]
	then
		fg="Yellow"
	fi

	if [ $j -eq 4 ]
	then
		fg="Blue"
	fi

	if [ $j -eq 5 ]
	then
		fg="Magenta"
	fi

	if [ $j -eq 6 ]
	then
		fg="Cyan"
	fi

	if [ $j -eq 7 ]
	then
		fg="White"
	fi

	for k in {0..7}
	do

        	if [ $k -eq 0 ]
        	then
            		bg="Black"
        	fi

        	if [ $k -eq 1 ]
        	then
            		bg="Red"
        	fi

        	if [ $k -eq 2 ]
        	then
            		bg="Green"
        	fi

        	if [ $k -eq 3 ]
        	then
            		bg="Yellow"
        	fi

        	if [ $k -eq 4 ]
        	then
            		bg="Blue"
        	fi

        	if [ $k -eq 5 ]
        	then
            		bg="Magenta"
        	fi

        	if [ $k -eq 6 ]
        	then
            		bg="Cyan"
        	fi

        	if [ $k -eq 7 ]
        	then
            		bg="White"
        	fi

        	COLORPAIR=$(( ( ((7 - $j) * 8 ) + k ) << 8 ))
        	COLORPAIRBOLD=$(( $COLORPAIR | (( 1 << 21 )) ))             	#Extra bright / bold
        	COLORPAIRUND=$(( $COLORPAIR | (( 1 << 17 )) ))            	#Underline
        	COLORPAIRBLINK=$(( $COLORPAIR | (( 1 << 19 )) ))             	#Blink
       		COLORPAIRDIM=$(( $COLORPAIR | (( 1 << 20 )) ))                 	#Half bright

         	ARR[$CTR]="($fg,$bg)\nNormal: $COLORPAIR \nBold: $COLORPAIRBOLD \nUnderline: $COLORPAIRUND \nBlink: $COLORPAIRBLINK \nDim: $COLORPAIRDIM\n"
		let "CTR++"
	done
done

INP=0

function FUNC_MENU {

	echo "*****************************"
	echo "**CURSES COLOR VALUE MATRIX**"
	echo "*****************************"
	echo
	echo "To print full list of values, enter P"
	echo "To print all plain attribute values, enter A"
	echo "To print a list of values for a specified foreground color, enter its corresponding number"
	echo "To quit, enter Q"
	echo
	echo "Black - 1"
	echo "Red - 2"
	echo "Green - 3"
	echo "Yellow - 4"
	echo "Blue - 5"
	echo "Magenta - 6"
	echo "Cyan - 7"
	echo "White - 8"
	echo
}

FUNC_MENU

while [[ $INP != "Q" && $INP != "q" ]]
do
	if [[ $INP > 0 && $INP < 9 ]]
	then
		for l in {1..8}
		do	declare -i VAR=$INP
			VAR=$(($VAR - 1))
			m=$(( l+($VAR * 8) ))
			echo -e "${ARR[m]}"
		done
	fi

	if [[ $INP == "P" || $INP == "p" ]]
	then
		for n in {1..64}
		do
			echo -e "${ARR[n]}"
		done
	fi

	if [[ $INP == "A" || $INP == "a" ]]
	then
		echo -e "Plain Attributes\nBold: $(( 1 << 21 ))\nUnderline: $(( 1 << 17 ))\nBlink: $(( 1 << 19 ))\nDim: $(( 1 << 20 ))\n"
	fi

	if [[ $INP == "M" || $INP == "m" ]]
	then
		FUNC_MENU
	fi

	echo "To reprint menu, enter M"
	echo
	echo "Selection: "
	read INP
	echo
done

