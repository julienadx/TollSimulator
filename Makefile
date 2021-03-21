CC = gcc
SOURCE = Audoux_Julien_projet_SY40.c
PROJECT = Audoux_Julien_projet_SY40

$(PROJECT): $(SOURCE)
	$(CC) -o $(PROJECT) $(SOURCE)