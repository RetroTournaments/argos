mkdir -p /home/matthew/.argos/rec/
rsync -a nesbox01:~/repos/rgms/rec/ /home/matthew/.argos/rec/ -v
rsync -a nesbox02:~/repos/rgms/rec/ /home/matthew/.argos/rec/ -v
rsync -a nesbox03:~/repos/rgms/rec/ /home/matthew/.argos/rec/ -v
rsync -a nesbox04:~/repos/rgms/rec/ /home/matthew/.argos/rec/ -v
rsync -a nesbox01:~/.argos/rec/ /home/matthew/.argos/rec/ -v
rsync -a nesbox02:~/.argos/rec/ /home/matthew/.argos/rec/ -v
rsync -a nesbox03:~/.argos/rec/ /home/matthew/.argos/rec/ -v
rsync -a nesbox04:~/.argos/rec/ /home/matthew/.argos/rec/ -v
