tmux new-session -d 'ssh nesbox01' \; \
    split-window -h -p 50 'ssh nesbox02' \; \
    bind-key X set-window-option synchronize-panes\; display-message "synchronize-panes is now #{?pane_synchronized,on,off}" \; \
    attach-session -d

#tmux new-session -d 'ssh nesbox01' \; \
#    split-window -h -p 60 'ssh nesbox02' \; \
#    split-window -h -p 50 'ssh nesbox03' \; \
#    bind-key X set-window-option synchronize-panes\; display-message "synchronize-panes is now #{?pane_synchronized,on,off}" \; \
#    attach-session -d

#tmux new-session -d 'ssh nesbox01' \; \
#    split-window -h -p 75 'ssh nesbox02' \; \
#    split-window -h -p 75 'ssh nesbox03' \; \
#    split-window -h -p 50 'ssh nesbox04' \; \
#    bind-key X set-window-option synchronize-panes\; display-message "synchronize-panes is now #{?pane_synchronized,on,off}" \; \
#    attach-session -d
