# author: Chekai Chang
# ID    : 933612783

import string       # import the string library
import random       # import the random library


FileId= ['Chikorita', 'Bayleef', 'Meganium']	# create an array and store the three file name


# create a function which create ten random alphabet
def GenRmD():
   var1 = ""	#first we create a empty space
   for i in range(10):		#run random ten times using for loop
      var1 = var1 + (random.choice(string.ascii_lowercase))		#word append
   return var1		#return the space

# run three times to generate three files which create a file and write in
for i in range(3):
   DOC = open(FileId[i],"w")	#open a file which purpose is write
   print (GenRmD());		#print the content showing on the screen
   DOC .write(GenRmD()+"\n");	#print the content in NameArray[i]
   DOC .close()					#close the file



# generate a number and range is from 1 to 42, put in the variable x
x = random.randint(1,42) #Range is 1 to 42
print (x)	# print the x

# generate a number and range is from 1 to 42, put in the variable y
y = random.randint(1,42)
print (y)	# print the x

# generate a number and range is from 1 to 42, put in the variable z
z = x * y #Get product of the two random numbers
print (z)	# print the x
