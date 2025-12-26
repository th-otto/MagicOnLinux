..\bin\pasm -v -b -d=COUNTRY=1 -i=..\inc -i=..\hostbios ..\hostbios\hostbios.s -o=DE\hostbios.o
..\bin\pasm -v -b -d=COUNTRY=2 -i=..\inc -i=..\hostbios ..\hostbios\hostbios.s -o=FR\hostbios.o
..\bin\pasm -v -b -d=COUNTRY=3 -i=..\inc -i=..\hostbios ..\hostbios\hostbios.s -o=EN\hostbios.o

..\bin\aln -v -o DE\MAGICLIN.OS -c DE.INP
..\bin\aln -v -o EN\MAGICLIN.OS -c EN.INP
..\bin\aln -v -o FR\MAGICLIN.OS -c FR.INP
