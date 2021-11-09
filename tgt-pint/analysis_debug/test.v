module vc8000d_g2_HevcGetNeighborFlagsLu(
    ctb_size_div4,
    cu_size_pixel,
    tu_size_4x4,
    pixel_position_x_4x4,
    pixel_position_y_4x4,
    tu_x,
    tu_y,
    cu_x,
    cu_y,
    tmp_hor,
    tmp_ver,
    constrained_pred,
    slice_and_tile_info,
    state_in,
    state_out,
    neighbor_flags);
  input  [4:0]   ctb_size_div4;
  input  [6:0]   cu_size_pixel;
  input  [3:0]   tu_size_4x4;
  input  [3:0]   pixel_position_x_4x4;
  input  [3:0]   pixel_position_y_4x4;
  input  [5:0]   tu_x;
  input  [5:0]   tu_y;
  input  [5:0]   cu_x;
  input  [5:0]   cu_y;
  input  [10:0]  tmp_hor;
  input  [10:0]  tmp_ver;
  input  [20:0]  constrained_pred;
  input  [3:0]   slice_and_tile_info;
  input  [207:0] state_in;
  output [207:0] state_out;
  output [32:0]  neighbor_flags;
  wire [3:0]  vsi_s0 = pixel_position_x_4x4;
  wire [3:0]  vsi_s1 = pixel_position_y_4x4;
  reg  [23:0] vsi_s2;
  reg  [15:0] vsi_s3;
  reg  [3:0]  hor_edges_out[15:0];
  reg  [3:0]  ver_edges_out[15:0];
  wire [23:0] vsi_s4;
  wire [15:0] vsi_s5;
  wire [23:0] vsi_s6;
  wire [15:0] vsi_s7;
  wire [3:0]  hor_edges[15:0];
  wire [3:0]  ver_edges[15:0];
  wire [23:0] vsi_s8;
  wire [15:0] vsi_s9;
  wire [23:0] vsi_s10;
  wire [15:0] vsi_s11;
  wire        vsi_s12;
  genvar k;
  integer i;
  generate for (k=0;k<24;k=k+1)
    begin : above_constrained_pred_asssign
      assign  vsi_s10[k] = constrained_pred[k/2];
    end
  endgenerate
  generate for (k=0;k<16;k=k+1)
    begin : left_constrained_pred_assign
      assign  vsi_s11[k] = constrained_pred[12+k/2];
    end
  endgenerate
  assign vsi_s12 = constrained_pred[20];
  assign vsi_s6 = state_in[23:0];
  assign vsi_s7 = state_in[39:24];
  generate for (k=0;k<16;k=k+1)
    begin : edges
      assign hor_edges[k] = state_in[ (40+k*4) +: 4];
      assign ver_edges[k] = state_in[ (104+k*4) +: 4];
    end
  endgenerate
  assign vsi_s8 = state_in[191:168];
  assign vsi_s9 = state_in[207:192];
  reg [207:0] state_out;
  always @(*)
    begin
      state_out[23:0] = vsi_s2;
      state_out[39:24] = vsi_s3;
      for (i=0;i<16;i=i+1)
        begin  
          state_out[ (i*4+40) +: 4] = hor_edges_out[i];
          state_out[ (i*4+104) +: 4] = ver_edges_out[i];
        end      
      state_out[191:168] = vsi_s4;
      state_out[207:192] = vsi_s5;
    end
  wire vsi_s13 = (tu_x == 0) && (tu_y == 0);
  wire vsi_s14 = (cu_x == 0) && (cu_y == 0);
  wire vsi_s15 = slice_and_tile_info[0];
  wire vsi_s16 = slice_and_tile_info[2];
  wire vsi_s17 = slice_and_tile_info[3];
  reg [23:0] vsi_s18;
  reg [15:0] vsi_s19;
  always @(*)
    begin
      case (cu_size_pixel[6:2])
        5'h1: vsi_s19 = 16'h1;
        5'h2: vsi_s19 = 16'h3;
        5'h4: vsi_s19 = 16'hf;
        5'h8: vsi_s19 = 16'hff;
        default: vsi_s19 = 16'hffff;
      endcase
    end
  always @(*)
    begin
      case (vsi_s0) 
        4'h0: vsi_s18 = {8'b0,vsi_s19};
        4'h1: vsi_s18 = {7'b0,vsi_s19,1'b0};
        4'h2: vsi_s18 = {6'b0,vsi_s19,2'b0};
        4'h3: vsi_s18 = {5'b0,vsi_s19,3'b0};
        4'h4: vsi_s18 = {4'b0,vsi_s19,4'b0};
        4'h5: vsi_s18 = {3'b0,vsi_s19,5'b0};
        4'h6: vsi_s18 = {2'b0,vsi_s19,6'b0};
        4'h7: vsi_s18 = {1'b0,vsi_s19,7'b0};
        4'h8: vsi_s18 = {vsi_s19,8'b0};
        4'h9: vsi_s18 = {vsi_s19[14:0],9'b0};
        4'ha: vsi_s18 = {vsi_s19[13:0],10'b0};
        4'hb: vsi_s18 = {vsi_s19[12:0],11'b0};
        4'hc: vsi_s18 = {vsi_s19[11:0],12'b0};
        4'hd: vsi_s18 = {vsi_s19[10:0],13'b0};
        4'he: vsi_s18 = {vsi_s19[9:0],14'b0};
        default: vsi_s18 = {vsi_s19[8:0],15'b0};
      endcase
    end
  wire [4:0] vsi_s20 = cu_size_pixel[6:2] + {1'b0,vsi_s0}; 
  reg [23:0] vsi_s21;
  always @(*)
    begin
      case (vsi_s20) 
        5'h0: vsi_s21 = 24'hff;
        5'h1: vsi_s21 = {23'hff,1'b0};
        5'h2: vsi_s21 = {22'hff,2'b0};
        5'h3: vsi_s21 = {21'hff,3'b0};
        5'h4: vsi_s21 = {20'hff,4'b0};
        5'h5: vsi_s21 = {19'hff,5'b0};
        5'h6: vsi_s21 = {18'hff,6'b0};
        5'h7: vsi_s21 = {17'hff,7'b0};
        5'h8: vsi_s21 = {16'hff,8'b0};
        5'h9: vsi_s21 = {15'hff,9'b0};
        5'ha: vsi_s21 = {14'hff,10'b0};
        5'hb: vsi_s21 = {13'hff,11'b0};
        5'hc: vsi_s21 = {12'hff,12'b0};
        5'hd: vsi_s21 = {11'hff,13'b0};
        5'he: vsi_s21 = {10'hff,14'b0};
        5'hf: vsi_s21 = {9'hff,15'b0};
        5'h10: vsi_s21 = {8'hff,16'b0};
        5'h11: vsi_s21 = {7'h7f,17'b0};
        5'h12: vsi_s21 = {6'h3f,18'b0};
        5'h13: vsi_s21 = {5'h1f,19'b0};
        5'h14: vsi_s21 = {4'hf,20'b0};
        5'h15: vsi_s21 = {3'h7,21'b0};
        5'h16: vsi_s21 = {2'h3,22'b0};
        5'h17: vsi_s21 = {1'h1,23'b0};
        default :  vsi_s21 = 24'b0;
      endcase
    end
  reg [15:0] vsi_s22;
  always @(*)
    begin
      case (ctb_size_div4)
        5'h1: vsi_s22 = 16'h1;
        5'h2: vsi_s22 = 16'h3;
        5'h4: vsi_s22 = 16'hf;
        5'h8: vsi_s22 = 16'hff;
        default: vsi_s22 = 16'hffff;
      endcase
    end
  wire [4:0] vsi_s23 = (tmp_hor > 24) ? 5'd24 : tmp_hor[4:0];
  wire [4:0] vsi_s24 = (tmp_ver > 16) ? 5'd16 : tmp_ver[4:0];
  reg  [23:0] vsi_s25;
  reg  [15:0] vsi_s26;
  always @(*)
    begin
      case (vsi_s23)
        5'd1  : vsi_s25 = 24'h1;
        5'd2  : vsi_s25 = 24'h3;
        5'd3  : vsi_s25 = 24'h7;
        5'd4  : vsi_s25 = 24'hf;
        5'd5  : vsi_s25 = 24'h1f;
        5'd6  : vsi_s25 = 24'h3f;
        5'd7  : vsi_s25 = 24'h7f;
        5'd8  : vsi_s25 = 24'hff;
        5'd9  : vsi_s25 = 24'h1ff;
        5'd10 : vsi_s25 = 24'h3ff;
        5'd11 : vsi_s25 = 24'h7ff;
        5'd12 : vsi_s25 = 24'hfff;
        5'd13 : vsi_s25 = 24'h1fff;
        5'd14 : vsi_s25 = 24'h3fff;
        5'd15 : vsi_s25 = 24'h7fff;
        5'd16 : vsi_s25 = 24'hffff;
        5'd17 : vsi_s25 = 24'h1ffff;
        5'd18 : vsi_s25 = 24'h3ffff;
        5'd19 : vsi_s25 = 24'h7ffff;
        5'd20 : vsi_s25 = 24'hfffff;
        5'd21 : vsi_s25 = 24'h1fffff;
        5'd22 : vsi_s25 = 24'h3fffff;
        5'd23 : vsi_s25 = 24'h7fffff;
        default : vsi_s25 = 24'hffffff;
      endcase
    end
  always @(*)
    begin
      case (vsi_s24)
        5'd1  : vsi_s26 = 16'h1;
        5'd2  : vsi_s26 = 16'h3;
        5'd3  : vsi_s26 = 16'h7;
        5'd4  : vsi_s26 = 16'hf;
        5'd5  : vsi_s26 = 16'h1f;
        5'd6  : vsi_s26 = 16'h3f;
        5'd7  : vsi_s26 = 16'h7f;
        5'd8  : vsi_s26 = 16'hff;
        5'd9  : vsi_s26 = 16'h1ff;
        5'd10 : vsi_s26 = 16'h3ff;
        5'd11 : vsi_s26 = 16'h7ff;
        5'd12 : vsi_s26 = 16'hfff;
        5'd13 : vsi_s26 = 16'h1fff;
        5'd14 : vsi_s26 = 16'h3fff;
        5'd15 : vsi_s26 = 16'h7fff;
        default: vsi_s26 = 16'hffff;
      endcase
    end
  assign vsi_s4 = (vsi_s13 && vsi_s14) ? vsi_s25 : vsi_s8;
  assign vsi_s5 = (vsi_s13 && vsi_s14) ? vsi_s26 : vsi_s9;
  always @(*)
    begin
      if (vsi_s13)
        begin
          vsi_s2 = 24'd0;
          if (vsi_s16)
            begin
              vsi_s2 = vsi_s18;
            end
          else 
            begin
              vsi_s2 = vsi_s2;
            end
          if (vsi_s17)
            begin
              vsi_s2 = vsi_s2 | vsi_s21;
            end
          else 
            begin
              vsi_s2 = vsi_s2;
            end
          vsi_s2 = vsi_s2 & vsi_s4;
          vsi_s2 = vsi_s2 & vsi_s10;
        end
      else
        begin
          vsi_s2 = vsi_s6;
        end
    end
  always @(*)
    begin
      if (vsi_s13)
        begin
          vsi_s3 = 16'd0;
          if (vsi_s15)
            begin
              vsi_s3 = vsi_s22;
            end
          else 
            begin
              vsi_s3 = vsi_s3;
            end
          vsi_s3 = vsi_s3 & vsi_s5;
          vsi_s3 = vsi_s3 & vsi_s11;
        end
      else
        begin
          vsi_s3 = vsi_s7;
        end
    end
  wire [4:0] vsi_s27 = {1'b0, vsi_s0} - 1'b1;
  wire [3:0] vsi_s28 = vsi_s27[3:0];
  wire [3:0] vsi_s29 = vsi_s1 - 1'b1;  
  reg vsi_s30;
  always @(*)  
    begin
      if (vsi_s13) 
        vsi_s30 = slice_and_tile_info[1] & vsi_s12;
      else if (tu_y == 0) 
        vsi_s30 = slice_and_tile_info[2] & vsi_s10[vsi_s27];
      else if (tu_x == 0) 
        vsi_s30 = slice_and_tile_info[0] & vsi_s11[vsi_s29];
      else 
        vsi_s30 = 1'b1;
    end
  wire [5:0] vsi_s31 = {2'b0,hor_edges[vsi_s29]} + 1'b1 - {2'b0,vsi_s0};
  wire [5:0] vsi_s32 = {2'b0,ver_edges[vsi_s28]} + 1'b1 - {2'b0,vsi_s1};
  reg  [15:0] vsi_s33;
  reg  [15:0] vsi_s34;
  always @(*)
    begin
      if (vsi_s31[5])
        begin
          vsi_s33 = 16'b0;
        end
      else  
        begin
          case (vsi_s31[4:0])
            5'h0:     vsi_s33 = 16'h0;
            5'h1:     vsi_s33 = 16'h1;
            5'h2:     vsi_s33 = 16'h3;
            5'h3:     vsi_s33 = 16'h7;
            5'h4:     vsi_s33 = 16'hf;
            5'h5:     vsi_s33 = 16'h1f;
            5'h6:     vsi_s33 = 16'h3f;
            5'h7:     vsi_s33 = 16'h7f;
            5'h8:     vsi_s33 = 16'hff;
            5'h9:     vsi_s33 = 16'h1ff;
            5'ha:     vsi_s33 = 16'h3ff;
            5'hb:     vsi_s33 = 16'h7ff;
            5'hc:     vsi_s33 = 16'hfff;
            5'hd:     vsi_s33 = 16'h1fff;
            5'he:     vsi_s33 = 16'h3fff;
            5'hf:     vsi_s33 = 16'h7fff;
            default:  vsi_s33 = 16'hffff;
          endcase
        end
    end
  always @(*)
    begin
      if (vsi_s32[5])
        begin
          vsi_s34 = 16'b0;
        end
      else  
        begin
          case (vsi_s32[4:0])
            5'h0:     vsi_s34 = 16'h0;
            5'h1:     vsi_s34 = 16'h1;
            5'h2:     vsi_s34 = 16'h3;
            5'h3:     vsi_s34 = 16'h7;
            5'h4:     vsi_s34 = 16'hf;
            5'h5:     vsi_s34 = 16'h1f;
            5'h6:     vsi_s34 = 16'h3f;
            5'h7:     vsi_s34 = 16'h7f;
            5'h8:     vsi_s34 = 16'hff;
            5'h9:     vsi_s34 = 16'h1ff;
            5'ha:     vsi_s34 = 16'h3ff;
            5'hb:     vsi_s34 = 16'h7ff;
            5'hc:     vsi_s34 = 16'hfff;
            5'hd:     vsi_s34 = 16'h1fff;
            5'he:     vsi_s34 = 16'h3fff;
            5'hf:     vsi_s34 = 16'h7fff;
            default:  vsi_s34 = 16'hffff;
          endcase
        end
    end
  wire [39:0]    vsi_s35 = {16'b0,vsi_s2};
  wire [31:0]    vsi_s36 = {16'b0,vsi_s3};
  wire [23:0]    vsi_s37 = vsi_s35[vsi_s0 +: 24];
  wire [15:0]    vsi_s38 = vsi_s36[vsi_s1 +: 16];
  wire [15:0] vsi_s39 = (tu_size_4x4==4'h1)?16'h1:(tu_size_4x4==4'h2)?16'h3:(tu_size_4x4==4'h4)?16'hf:16'hff;
  wire [15:0] vsi_s40 = (tu_size_4x4==4'h1)?16'h3:(tu_size_4x4==4'h2)?16'hf:(tu_size_4x4==4'h4)?16'hff:16'hffff;
  wire [15:0] vsi_s41 = (tu_size_4x4==4'h1)?16'h2:(tu_size_4x4==4'h2)?16'hc:(tu_size_4x4==4'h4)?16'hf0:16'hff00;
  wire [15:0]  vsi_s42 = (tu_y == 0)? (vsi_s37[15:0] & vsi_s40):(vsi_s33 & vsi_s40);
  wire [15:0]  vsi_s43 = (tu_x == 0)? (vsi_s38 & vsi_s39):(vsi_s34 & vsi_s39);
  reg  [15:0]  vsi_s44;
  wire [31:0]  vsi_s45 = {16'b0,vsi_s11};
  wire [15:0]  vsi_s46 = vsi_s45[vsi_s1 +: 16];
  
  always @(*)
    begin // up(vsi_s43), up(vsi_s0), up(vsi_s41)
      //#1;
      if (vsi_s0 == 0) 
        begin // up(vsi_s38)
          vsi_s44 = vsi_s38 & vsi_s41;
          vsi_s44 = vsi_s44 | vsi_s43; // down(vsi_s44)
        end
      else
        begin
          vsi_s44 = vsi_s34 & vsi_s41; // up(tu_x), up(vsi_s34)
          if(tu_x == 0)
            begin // true-part: up(vsi_s46)
              //@(vsi_s0)
              vsi_s44 = vsi_s44 & vsi_s46;
            end
          vsi_s44 = vsi_s44 | vsi_s43; // down(vsi_s44)
        end      
    end
  
    initial begin
      if (vsi_s0 == 0) 
        begin
          vsi_s44 = vsi_s38 & vsi_s41;
          vsi_s44 = vsi_s44 | vsi_s43; 
        end
      else
        begin
          vsi_s44 = vsi_s34 & vsi_s41;
          if(tu_x == 0)
            begin
              vsi_s44 = vsi_s44 & vsi_s46;
            end
          vsi_s44 = vsi_s44 | vsi_s43;
        end      
    end

  reg [15:0] vsi_s47;
  always @(*)
    begin
      for (i=0;i<16;i=i+1)
        vsi_s47[i] = vsi_s44[15-i];
    end
  assign neighbor_flags = { vsi_s42, vsi_s30, vsi_s47 };
  wire [3:0] vsi_s48 = vsi_s0 + tu_size_4x4 - 1'b1;
  wire [3:0] vsi_s49 = vsi_s1 + tu_size_4x4 - 1'b1;
  always @(*)
    begin
      for (i=0 ; i<16; i=i+1 )
        begin
          hor_edges_out[i] = (vsi_s1 <= i && vsi_s49 >= i) ? vsi_s48 : (vsi_s13 && vsi_s14) ? 4'd0 : hor_edges[i];
          ver_edges_out[i] = (vsi_s0 <= i && vsi_s48 >= i) ? vsi_s49 : (vsi_s13 && vsi_s14) ? 4'd0 : ver_edges[i];
        end
    end
endmodule
