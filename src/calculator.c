#include <reg51.h>

// LCD control pins
sbit RS = P3^5;
sbit RW = P3^6;
sbit EN = P3^7;

#define INF      9999
#define NEG_INF -9999

// keypad mapping as per design
code char keys[16] = {
    '1','2','3','A',
    '4','5','6','B',
    '7','8','9','C',
    '*','0','#','D'
};

//Lookup table for sin()
code unsigned int sin_table[91] = {
0,17,35,52,70,87,105,122,139,156,
173,190,207,224,241,258,275,292,309,325,
342,358,374,390,406,422,438,454,469,485,
500,515,530,545,559,574,588,602,616,629,
643,656,669,682,695,707,719,731,743,754,
766,777,788,799,809,819,829,839,848,857,
866,875,883,891,899,906,914,921,927,934,
940,946,951,956,961,966,970,974,978,981,
985,987,990,993,995,996,998,999,999,1000,1000
};

// trigonometric function names
code char trig_labels[4][5] = {
    "sin(", "cos(", "tan(", "cot("
};

//custom delay function
void delay_ms(unsigned int ms){
    unsigned int i,j;
    for(i=0;i<ms;i++)
        for(j=0;j<1275;j++);
}

//LCD functions
void lcd_enable(){ EN=1; delay_ms(1); EN=0; }

void lcd_cmd(unsigned char cmd){ P1=cmd; RS=0; RW=0; lcd_enable(); }

void lcd_data(unsigned char dat){ P1=dat; RS=1; RW=0; lcd_enable(); }

void lcd_init(){
    delay_ms(20);
    lcd_cmd(0x38); lcd_cmd(0x0C);
    lcd_cmd(0x06); lcd_cmd(0x01);
    delay_ms(2);
}

void lcd_str(char *s){ while(*s) lcd_data(*s++); }

//Display any integer by iterating digit by digit
void disp_num(int num){
    char buf[6];
    int i=0, len;
    if(num==0){ lcd_data('0'); return; }
    if(num<0){ lcd_data('-'); num=-num; }
    while(num>0){ buf[i++]=(num%10)+'0'; num/=10; }
    len=i;
    for(i=len-1;i>=0;i--) lcd_data(buf[i]);
}

//Special display function for trig funcs
void disp_scaled(int val){
    if(val==INF||val==NEG_INF){ lcd_str("INF"); return; }
    if(val<0){ lcd_data('-'); val=-val; }
    lcd_data((val/1000)+'0');
    lcd_data('.');
    val%=1000;
    lcd_data((val/100)+'0');
    lcd_data(((val/10)%10)+'0');
    lcd_data((val%10)+'0');
}

//Keypad scanning algorithm
char get_key(){
    unsigned char row,col;
    while(1){
        for(row=0;row<4;row++){
            P0|=0x0F; P0&=~(1<<row);
            col=P0&0xF0;
            if(col!=0xF0){
                delay_ms(20); col=P0&0xF0;
                if(col!=0xF0){
                    if(!(col&0x10)) return keys[row*4+0];
                    if(!(col&0x20)) return keys[row*4+1];
                    if(!(col&0x40)) return keys[row*4+2];
                    if(!(col&0x80)) return keys[row*4+3];
                    while((P0&0xF0)!=0xF0);
                }
            }
        }
    }
}

//Normalise any angle to known angle (0-90)
int norm(int a){ a%=360; if(a<0) a+=360; return a; }

int get_sin(int a){
    a=norm(a);
    if(a<=90)  return  sin_table[a];
    if(a<=180) return  sin_table[180-a];
    if(a<=270) return -(int)sin_table[a-180];
    return             -(int)sin_table[360-a];
}

int get_cos(int a){ return get_sin((90-a+360)%360); }

// Shared divide helper eliminates duplicate long arithmetic in tan/cot
int trig_div(long num, long den){
    long r;
    if(den==0) return INF;
    r=(num*1000L)/den;
    if(r> 9999) return  INF;
    if(r<-9999) return  NEG_INF;
    return (int)r;
}

int get_tan(int a){ return trig_div(get_sin(a),get_cos(a)); }
int get_cot(int a){ return trig_div(get_cos(a),get_sin(a)); }

// find exponent of a number
int power(int base,int exp){
    long res=1; int i;
    if(exp<0) return 0;
    for(i=0;i<exp;i++){
        res*=base;
        if(res> 32767) return  32767;
        if(res<-32768) return -32768;
    }
    return (int)res;
}


void main(){
    int  num1=0,num2=0,result=0;
    char op=0,mode=0,trig_func=0;
    bit  second=0;

    lcd_init();

    while(1){
        char key=get_key();

        if(key=='A'||key=='D'){ mode=key; continue; }

        if(mode=='A'){
            switch(key){
                case '1': op='+'; break;
                case '2': op='-'; break;
                case '3': op='/'; break;
                case '4': op='*'; break;
                default:  mode=0; continue;
            }
            lcd_data(op); second=1; mode=0;
        }

        else if(mode=='D'){
            unsigned char idx;
            switch(key){
                case '1': idx=0; trig_func='S'; break;
                case '2': idx=1; trig_func='C'; break;
                case '3': idx=2; trig_func='T'; break;
                case '4': idx=3; trig_func='O'; break;
                default:  mode=0; continue;
            }
            lcd_cmd(0x01);
            lcd_str(trig_labels[idx]);
            num1=0; second=0; mode='T';
        }

        else if(key>='0'&&key<='9'){
            if(!second) num1=num1*10+(key-'0');
            else        num2=num2*10+(key-'0');
            lcd_data(key);
        }

        else if(key=='*'){ op='^'; second=1; lcd_data('^'); }

        else if(key=='#'){
            if(mode=='T'){
                int val;
                switch(trig_func){
                    case 'S': val=get_sin(num1); break;
                    case 'C': val=get_cos(num1); break;
                    case 'T': val=get_tan(num1); break;
                    default:  val=get_cot(num1); break;
                }
								lcd_data(')');
                lcd_cmd(0xC0); disp_scaled(val);  // 0xC0 = row 2 start
                num1=0; mode=0; trig_func=0;
                continue;
            }
            switch(op){
                case '+': result=num1+num2; break;
                case '-': result=num1-num2; break;
                case '*': result=num1*num2; break;
                case '/':
                    if(num2==0){
                        lcd_cmd(0xC0); lcd_str("DIV ERR");  // row 2
                        num1=num2=result=0; op=0; second=0;
                        continue;
                    }
                    result=num1/num2; break;
                case '^': result=power(num1,num2); break;
                default: continue;
            }
            lcd_cmd(0xC0); disp_num(result);  // 0xC0 = row 2 start
            num1=result; num2=0; second=0;
        }

        else if(key=='B'){
            if(!second) num1/=10; else num2/=10;
            lcd_cmd(0x01); disp_num(num1);
            if(second){ lcd_data(op); disp_num(num2); }
        }

        else if(key=='C'){
            num1=num2=result=0;
            op=0; second=0; mode=0; trig_func=0;
            lcd_cmd(0x01); delay_ms(2);
        }
    }
}