// arm_multi.sv
// David_Harris@hmc.edu, Sarah_Harris@hmc.edu 25 December 2013
// Multi-cycle implementation of a subset of ARMv4

// 16 32-bit registers
// Data-processing instructions
//   ADD, SUB, AND, ORR
//   INSTR <cond> <S> <Rd>, <Rn>, #immediate
//   INSTR <cond> <S> <Rd>, <Rn>, <Rm>
//    Rd <- <Rn> INSTR <Rm>	    	if (S) Update Status Flags
//    Rd <- <Rn> INSTR immediate	if (S) Update Status Flags
//   Instr[31:28] = cond
//   Instr[27:26] = Op = 00
//   Instr[25:20] = Funct
//                  [25]:    1 for immediate, 0 for register
//                  [24:21]: 0100 (ADD) / 0010 (SUB) /
//                           0000 (AND) / 1100 (ORR)
//                  [20]:    S (1 = update CPSR status Flags)
//   Instr[19:16] = Rn
//   Instr[15:12] = Rd
//   Instr[11:8]  = 0000
//   Instr[7:0]   = immed_8  (for #immediate type) / 
//                  0000<Rm> (for register type)
//   
// Load/Store instructions
//   LDR, STR
//   INSTR <Rd>, [<Rn>, #offset]
//    LDR: Rd <- Mem[<Rn>+offset]
//    STR: Mem[<Rn>+offset] <- Rd
//   Instr[31:28] = cond
//   Instr[27:26] = Op = 01 
//   Instr[25:20] = Funct
//                  [25]:    0 (A)
//                  [24:21]: 1100 (P/U/B/W)
//                  [20]:    L (1 for LDR, 0 for STR)
//   Instr[19:16] = Rn
//   Instr[15:12] = Rd
//   Instr[11:0]  = imm (zero extended)
//
// Branch instruction (PC <= PC + offset, PC holds 8 bytes past Branch
//   B
//   INSTR <target>
//    PC <- PC + 8 + imm << 2
//   Instr[31:28] = cond
//   Instr[27:25] = Op = 10
//   Instr[25:24] = Funct
//                  [25]: 1 (Branch)
//                  [24]: 0 (link)
//   Instr[23:0]  = offset (sign extend, shift left 2)
//   Note: no Branch delay slot on ARM
//
// Other:
//   R15 reads as PC+8
//   Conditional Encoding
//    cond  Meaning                       Flag
//    0000  Equal                         Z = 1
//    0001  Not Equal                     Z = 0
//    0010  Carry Set                     C = 1
//    0011  Carry Clear                   C = 0
//    0100  Minus                         N = 1
//    0101  Plus                          N = 0
//    0110  Overflow                      V = 1
//    0111  No Overflow                   V = 0
//    1000  Unsigned Higher               C = 1 & Z = 0
//    1001  Unsigned Lower/Same           C = 0 | Z = 1
//    1010  Signed greater/equal          N = V
//    1011  Signed less                   N != V
//    1100  Signed greater                N = V & Z = 0
//    1101  Signed less/equal             N != V | Z = 1
//    1110  Always                        any
//   Writes to register 15 (PC) are ignored 



module controller(input  logic         clk,
                  input  logic         reset,
                  input  logic [31:12] Instr,
                  input  logic [3:0]   ALUFlags,
                  output logic         PCWrite,
                  output logic         MemWrite,
                  output logic         RegWrite,
                  output logic         IRWrite,
                  output logic         AdrSrc,
                  output logic [1:0]   RegSrc,
                  output logic [1:0]   ALUSrcA,
                  output logic [1:0]   ALUSrcB,
                  output logic [1:0]   ResultSrc,
                  output logic [1:0]   ImmSrc,
                  output logic [1:0]   ALUControl);
                  
  logic [1:0] FlagW;
  logic       PCS, NextPC, RegW, MemW;
  
  decode dec(clk, reset, Instr[27:26], Instr[25:20], Instr[15:12],
             FlagW, PCS, NextPC, RegW, MemW,
             IRWrite, AdrSrc, ResultSrc, 
             ALUSrcA, ALUSrcB, ImmSrc, RegSrc, ALUControl);
  condlogic cl(clk, reset, Instr[31:28], ALUFlags,
               FlagW, PCS, NextPC, RegW, MemW,
               PCWrite, RegWrite, MemWrite);
endmodule

module decode(input  logic       clk, reset,
              input  logic [1:0] Op,
              input  logic [5:0] Funct,
              input  logic [3:0] Rd,
              output logic [1:0] FlagW,
              output logic       PCS, NextPC, RegW, MemW,
              output logic       IRWrite, AdrSrc,
              output logic [1:0] ResultSrc, ALUSrcA, ALUSrcB, 
              output logic [1:0] ImmSrc, RegSrc, ALUControl);

  logic       Branch, ALUOp;

  // Main FSM
  mainfsm fsm(clk, reset, Op, Funct, 
              IRWrite, AdrSrc, 
              ALUSrcA, ALUSrcB, ResultSrc,
              NextPC, RegW, MemW, Branch, ALUOp);

  // ADD CODE BELOW
  // Add code for the ALU Decoder and PC Logic.
  // Remember, you may reuse code from previous labs.
  // ALU Decoder

  // PC Logic


  // Add code for the Instruction Decoder (Instr Decoder) below.
  // Recall that the input to Instr Decoder is Op, and the outputs are
  // ImmSrc and RegSrc. We've completed the ImmSrc logic for you.

  // Instr Decoder
  assign ImmSrc    = Op;

endmodule

module mainfsm(input  logic         clk,
               input  logic         reset,
               input  logic [1:0]   Op,
               input  logic [5:0]   Funct,
               output logic         IRWrite,
               output logic         AdrSrc,
               output logic [1:0]   ALUSrcA, ALUSrcB, ResultSrc,
               output logic         NextPC, RegW, MemW, Branch, ALUOp);  
              
  typedef enum logic [3:0] {FETCH, DECODE, MEMADR, MEMRD, MEMWB, 
                            MEMWR, EXECUTER, EXECUTEI, ALUWB, BRANCH, 
				     UNKNOWN} 
statetype;
  
  statetype state, nextstate;
  logic [12:0] controls;
  
  // state register
  always @(posedge clk or posedge reset)
    if (reset) state <= FETCH;
    else state <= nextstate;
  
  // ADD CODE BELOW
  // Finish entering the next state logic below.  We've completed the 
  // first two states, FETCH and DECODE, for you.

  // next state logic
  always_comb
    casex(state)
      FETCH:                     nextstate = DECODE;
      DECODE: case(Op)
                2'b00: 
                  if (Funct[5])  nextstate = EXECUTEI;
                  else           nextstate = EXECUTER;
                2'b01:           nextstate = MEMADR;
                2'b10:           nextstate = BRANCH;
                default:         nextstate = UNKNOWN;
              endcase
      EXECUTER:                  
      EXECUTEI:                  
      MEMADR: 
      MEMRD:                     
      default:                   nextstate = FETCH; 
    endcase
    
  // ADD CODE BELOW
  // Finish entering the output logic below.  We've entered the
  // output logic for the first two states, FETCH and DECODE, for you.

  // state-dependent output logic
  always_comb
    case(state)
      FETCH: 	controls = 13'b10001_010_01100; 
      DECODE:  	controls = 13'b00000_010_01100;      
      EXECUTER: 
      EXECUTEI: 
      ALUWB: 
      MEMADR: 
      MEMWR: 
      MEMRD: 
      MEMWB: 
      BRANCH: 
      default: 	controls = 13'bxxxxx_xxx_xxxxx;
    endcase

  assign {NextPC, Branch, MemW, RegW, IRWrite,
          AdrSrc, ResultSrc,   
          ALUSrcA, ALUSrcB, ALUOp} = controls;
endmodule              

// ADD CODE BELOW
// Add code for the condlogic and condcheck modules. Remember, you may
// reuse code from prior labs.
module condlogic(input  logic       clk, reset,
                 input  logic [3:0] Cond,
                 input  logic [3:0] ALUFlags,
                 input  logic [1:0] FlagW,
                 input  logic       PCS, NextPC, RegW, MemW,
                 output logic       PCWrite, RegWrite, MemWrite);

  logic [1:0] FlagWrite;
  logic [3:0] Flags;
  logic       CondEx;


  // Delay writing flags until ALUWB state
  flopr #(2)flagwritereg(clk, reset, FlagW&{2{CondEx}}, FlagWrite);

  // ADD CODE HERE

endmodule    

module condcheck(input  logic [3:0] Cond,
                 input  logic [3:0] Flags,
                 output logic       CondEx);

  // ADD CODE HERE

endmodule



// ADD CODE BELOW
// Add needed building blocks below (i.e., parameterizable muxes, 
// registers, etc.). Remember, you can reuse code from previous labs.
// We've also provided a parameterizable 3:1 mux below for your 
// convenience.

module mux3 #(parameter WIDTH = 8)
             (input  logic [WIDTH-1:0] d0, d1, d2,
              input  logic [1:0]       s, 
              output logic [WIDTH-1:0] y);

  assign y = s[1] ? d2 : (s[0] ? d1 : d0); 
endmodule

