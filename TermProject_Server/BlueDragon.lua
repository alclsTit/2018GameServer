my_id = 99999;

function set_myid(x)
my_id = x;
end

function player_move(player_id, x, y)

my_x = API_get_x_position(my_id);
my_y = API_get_y_position(my_id);

if (x == my_x) then
	if (y == my_y) then
		API_send_chat_packet(player_id, my_id, "Catched!!!");  
	end
end


end 